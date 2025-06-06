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

#include "TextureUploader.hpp"
#include "DebugUtilities.hpp"

#if D3D11_SUPPORTED
#    include "TextureUploaderD3D11.hpp"
#endif

#if D3D12_SUPPORTED || VULKAN_SUPPORTED
#    include "TextureUploaderD3D12_Vk.hpp"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#    include "TextureUploaderGL.hpp"
#endif

#if WEBGPU_SUPPORTED
#    include "TextureUploaderWebGPU.hpp"
#endif

namespace Diligent
{

void CreateTextureUploader(IRenderDevice* pDevice, const TextureUploaderDesc& Desc, ITextureUploader** ppUploader)
{
    *ppUploader = nullptr;
    switch (pDevice->GetDeviceInfo().Type)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
            *ppUploader = MakeNewRCObj<TextureUploaderD3D11>()(pDevice, Desc);
            break;
#endif

#if D3D12_SUPPORTED || VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
        case RENDER_DEVICE_TYPE_VULKAN:
            *ppUploader = MakeNewRCObj<TextureUploaderD3D12_Vk>()(pDevice, Desc);
            break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GLES:
        case RENDER_DEVICE_TYPE_GL:
            *ppUploader = MakeNewRCObj<TextureUploaderGL>()(pDevice, Desc);
            break;
#endif

#if WEBGPU_SUPPORTED
        case RENDER_DEVICE_TYPE_WEBGPU:
            *ppUploader = MakeNewRCObj<TextureUploaderWebGPU>()(pDevice, Desc);
            break;
#endif
        default:
            UNEXPECTED("Unsupported device type");
    }
    if (*ppUploader != nullptr)
        (*ppUploader)->AddRef();
}

} // namespace Diligent
