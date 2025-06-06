/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "pch.h"

#include "Texture1D_GL.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "BufferGLImpl.hpp"
#include "GLTypeConversions.hpp"

namespace Diligent
{

Texture1D_GL::Texture1D_GL(IReferenceCounters*        pRefCounters,
                           FixedBlockMemoryAllocator& TexViewObjAllocator,
                           RenderDeviceGLImpl*        pDeviceGL,
                           GLContextState&            GLState,
                           const TextureDesc&         TexDesc,
                           const TextureData*         pInitData /*= nullptr*/,
                           bool                       bIsDeviceInternal /*= false*/) :
    // clang-format off
    TextureBaseGL
    {
        pRefCounters,
        TexViewObjAllocator,
        pDeviceGL,
        TexDesc,
        GL_TEXTURE_1D,
        pInitData,
        bIsDeviceInternal
    }
// clang-format on
{
    if (TexDesc.Usage == USAGE_STAGING)
    {
        // We will use PBO initialized by TextureBaseGL
        return;
    }

    GLState.BindTexture(-1, m_BindTarget, m_GlTexture);

    //                             levels             format          width
    glTexStorage1D(m_BindTarget, m_Desc.MipLevels, m_GLTexFormat, m_Desc.Width);
    DEV_CHECK_GL_ERROR_AND_THROW("Failed to allocate storage for the 1D texture");
    // When target is GL_TEXTURE_1D, calling glTexStorage1D is equivalent to the following pseudo-code:
    //for (i = 0; i < levels; i++)
    //{
    //    glTexImage1D(target, i, internalformat, width, 0, format, type, NULL);
    //    width = max(1, (width / 2));
    //}

    SetDefaultGLParameters();

    if (pInitData != nullptr && pInitData->pSubResources != nullptr)
    {
        if (m_Desc.MipLevels == pInitData->NumSubresources)
        {
            for (Uint32 Mip = 0; Mip < m_Desc.MipLevels; ++Mip)
            {
                Box DstBox{0, std::max(m_Desc.Width >> Mip, 1U),
                           0, 1};
                // UpdateData() is a virtual function. If we try to call it through vtbl from here,
                // we will get into TextureBaseGL::UpdateData(), because instance of Texture1D_GL
                // is not fully constructed yet.
                // To call the required function, we need to explicitly specify the class:
                Texture1D_GL::UpdateData(GLState, Mip, 0, DstBox, pInitData->pSubResources[Mip]);
            }
        }
        else
        {
            UNEXPECTED("Incorrect number of subresources");
        }
    }

    m_GlTexture.SetName(m_Desc.Name);

    GLState.BindTexture(-1, m_BindTarget, GLObjectWrappers::GLTextureObj::Null());
}

Texture1D_GL::Texture1D_GL(IReferenceCounters*        pRefCounters,
                           FixedBlockMemoryAllocator& TexViewObjAllocator,
                           RenderDeviceGLImpl*        pDeviceGL,
                           GLContextState&            GLState,
                           const TextureDesc&         TexDesc,
                           GLuint                     GLTextureHandle,
                           GLuint                     GLBindTarget,
                           bool                       bIsDeviceInternal) :
    // clang-format off
    TextureBaseGL
    {
        pRefCounters,
        TexViewObjAllocator,
        pDeviceGL,
        GLState,
        TexDesc,
        GLTextureHandle,
        static_cast<GLenum>(GLBindTarget != 0 ? GLBindTarget : GL_TEXTURE_1D),
        bIsDeviceInternal
    }
// clang-format on
{
}

Texture1D_GL::~Texture1D_GL()
{
}

void Texture1D_GL::UpdateData(GLContextState&          ContextState,
                              Uint32                   MipLevel,
                              Uint32                   Slice,
                              const Box&               DstBox,
                              const TextureSubResData& SubresData)
{
    TextureBaseGL::UpdateData(ContextState, MipLevel, Slice, DstBox, SubresData);

    ContextState.BindTexture(-1, m_BindTarget, m_GlTexture);

    // Bind buffer if it is provided; copy from CPU memory otherwise
    GLuint UnpackBuffer = 0;
    if (SubresData.pSrcBuffer != nullptr)
    {
        BufferGLImpl* pBufferGL = ClassPtrCast<BufferGLImpl>(SubresData.pSrcBuffer);
        UnpackBuffer            = pBufferGL->GetGLHandle();
    }

    // Transfers to OpenGL memory are called unpack operations
    // If there is a buffer bound to GL_PIXEL_UNPACK_BUFFER target, then all the pixel transfer
    // operations will be performed from this buffer.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, UnpackBuffer);

    const NativePixelAttribs& TransferAttribs = GetNativePixelTransferAttribs(m_Desc.Format);

    glPixelStorei(GL_UNPACK_ALIGNMENT, PBOOffsetAlignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    glTexSubImage1D(m_BindTarget, MipLevel,
                    DstBox.MinX,
                    DstBox.Width(),
                    TransferAttribs.PixelFormat, TransferAttribs.DataType,
                    // If a non-zero named buffer object is bound to the GL_PIXEL_UNPACK_BUFFER target, 'data' is treated
                    // as a byte offset into the buffer object's data store.
                    // https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glTexSubImage1D.xml
                    SubresData.pSrcBuffer != nullptr ? reinterpret_cast<void*>(StaticCast<size_t>(SubresData.SrcOffset)) : SubresData.pData);
    DEV_CHECK_GL_ERROR("Failed to update subimage data");

    if (UnpackBuffer != 0)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ContextState.BindTexture(-1, m_BindTarget, GLObjectWrappers::GLTextureObj::Null());
}

void Texture1D_GL::AttachToFramebuffer(const TextureViewDesc& ViewDesc, GLenum AttachmentPoint, FRAMEBUFFER_TARGET_FLAGS Targets)
{
    // For glFramebufferTexture1D(), if texture name is not zero, then texture target must be GL_TEXTURE_1D
    if (Targets & FRAMEBUFFER_TARGET_FLAG_DRAW)
    {
        VERIFY_EXPR(ViewDesc.ViewType == TEXTURE_VIEW_RENDER_TARGET || ViewDesc.ViewType == TEXTURE_VIEW_DEPTH_STENCIL);
        glFramebufferTexture1D(GL_DRAW_FRAMEBUFFER, AttachmentPoint, m_BindTarget, m_GlTexture, ViewDesc.MostDetailedMip);
        DEV_CHECK_GL_ERROR("Failed to attach texture 1D to draw framebuffer");
    }
    if (Targets & FRAMEBUFFER_TARGET_FLAG_READ)
    {
        glFramebufferTexture1D(GL_READ_FRAMEBUFFER, AttachmentPoint, m_BindTarget, m_GlTexture, ViewDesc.MostDetailedMip);
        DEV_CHECK_GL_ERROR("Failed to attach texture 1D to read framebuffer");
    }
}

void Texture1D_GL::CopyTexSubimage(GLContextState& GLState, const CopyTexSubimageAttribs& Attribs)
{
    GLState.BindTexture(-1, GetBindTarget(), GetGLHandle());

    glCopyTexSubImage1D(GetBindTarget(),
                        Attribs.DstMip,
                        Attribs.DstX,
                        Attribs.SrcBox.MinX,
                        Attribs.SrcBox.MinY,
                        Attribs.SrcBox.Width());
    DEV_CHECK_GL_ERROR("Failed to copy subimage data to texture 1D");
}

} // namespace Diligent
