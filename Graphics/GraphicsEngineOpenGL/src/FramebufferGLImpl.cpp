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

#include "FramebufferGLImpl.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "TextureViewGLImpl.hpp"
#include "FBOCache.hpp"
#include "GLContextState.hpp"

namespace Diligent
{

static bool UseDefaultFBO(Uint32             NumRenderTargets,
                          TextureViewGLImpl* ppRTVs[],
                          TextureViewGLImpl* pDSV)
{
    bool useDefaultFBO = false;
    for (Uint32 rt = 0; rt < NumRenderTargets; ++rt)
    {
        if (ppRTVs[rt] != nullptr && ppRTVs[rt]->GetHandle() == 0)
        {
            if (rt == 0)
            {
                useDefaultFBO = true;
            }
            else
            {
                LOG_ERROR_AND_THROW("In OpenGL, swap chain back buffer can be the only render target in the framebuffer "
                                    "and cannot be combined with any other render target.");
            }
        }
    }

    if (pDSV != nullptr)
    {
        if (useDefaultFBO && pDSV->GetHandle() != 0)
        {
            LOG_ERROR_AND_THROW("In OpenGL, swap chain back buffer can only be paired with the default depth-stencil buffer.");
        }

        if (pDSV->GetHandle() == 0)
        {
            if (!useDefaultFBO && NumRenderTargets > 0)
            {
                LOG_ERROR_AND_THROW("In OpenGL, the swap chain's depth-stencil buffer can only be paired with its back buffer.");
            }
            else
            {
                useDefaultFBO = true;
            }
        }
    }

    return useDefaultFBO;
}

FramebufferGLImpl::FramebufferGLImpl(IReferenceCounters*    pRefCounters,
                                     RenderDeviceGLImpl*    pDevice,
                                     const FramebufferDesc& Desc,
                                     GLContextState&        CtxState) :
    TFramebufferBase{pRefCounters, pDevice, Desc}
{
    const RenderPassDesc& RPDesc = m_Desc.pRenderPass->GetDesc();
    m_SubpassFramebuffers.reserve(RPDesc.SubpassCount);
    for (Uint32 subpass = 0; subpass < RPDesc.SubpassCount; ++subpass)
    {
        const SubpassDesc& SPDesc = RPDesc.pSubpasses[subpass];

        TextureViewGLImpl* ppRTVs[MAX_RENDER_TARGETS] = {};
        TextureViewGLImpl* pDSV                       = nullptr;

        for (Uint32 rt = 0; rt < SPDesc.RenderTargetAttachmentCount; ++rt)
        {
            const AttachmentReference& RTAttachmentRef = SPDesc.pRenderTargetAttachments[rt];
            if (RTAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
            {
                ppRTVs[rt] = ClassPtrCast<TextureViewGLImpl>(m_Desc.ppAttachments[RTAttachmentRef.AttachmentIndex]);
            }
        }

        if (SPDesc.pDepthStencilAttachment != nullptr && SPDesc.pDepthStencilAttachment->AttachmentIndex != ATTACHMENT_UNUSED)
        {
            pDSV = ClassPtrCast<TextureViewGLImpl>(m_Desc.ppAttachments[SPDesc.pDepthStencilAttachment->AttachmentIndex]);
        }
        GLObjectWrappers::GLFrameBufferObj RenderTargetFBO = UseDefaultFBO(SPDesc.RenderTargetAttachmentCount, ppRTVs, pDSV) ?
            GLObjectWrappers::GLFrameBufferObj{false} :
            FBOCache::CreateFBO(CtxState, SPDesc.RenderTargetAttachmentCount, ppRTVs, pDSV, Desc.Width, Desc.Height);

        GLObjectWrappers::GLFrameBufferObj ResolveFBO{false};
        if (SPDesc.pResolveAttachments != nullptr)
        {
            TextureViewGLImpl* ppRsvlViews[MAX_RENDER_TARGETS] = {};
            for (Uint32 rt = 0; rt < SPDesc.RenderTargetAttachmentCount; ++rt)
            {
                const AttachmentReference& RslvAttachmentRef = SPDesc.pResolveAttachments[rt];
                if (RslvAttachmentRef.AttachmentIndex != ATTACHMENT_UNUSED)
                {
                    ppRsvlViews[rt] = ClassPtrCast<TextureViewGLImpl>(m_Desc.ppAttachments[RslvAttachmentRef.AttachmentIndex]);
                }
            }
            ResolveFBO = UseDefaultFBO(SPDesc.RenderTargetAttachmentCount, ppRsvlViews, nullptr) ?
                GLObjectWrappers::GLFrameBufferObj{false} :
                FBOCache::CreateFBO(CtxState, SPDesc.RenderTargetAttachmentCount, ppRsvlViews, nullptr, Desc.Width, Desc.Height);
        }

        RenderTargetFBO.SetName(m_Desc.Name);

        m_SubpassFramebuffers.emplace_back(std::move(RenderTargetFBO), std::move(ResolveFBO));
    }
}

FramebufferGLImpl::~FramebufferGLImpl()
{
}

} // namespace Diligent
