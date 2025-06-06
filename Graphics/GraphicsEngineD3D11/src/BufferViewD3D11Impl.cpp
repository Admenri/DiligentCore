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
#include "BufferViewD3D11Impl.hpp"
#include "BufferD3D11Impl.hpp"
#include "RenderDeviceD3D11Impl.hpp"

namespace Diligent
{

BufferViewD3D11Impl::BufferViewD3D11Impl(IReferenceCounters*    pRefCounters,
                                         RenderDeviceD3D11Impl* pDevice,
                                         const BufferViewDesc&  ViewDesc,
                                         BufferD3D11Impl*       pBuffer,
                                         ID3D11View*            pD3D11View,
                                         bool                   bIsDefaultView) :
    // clang-format off
    TBufferViewBase
    {
        pRefCounters,
        pDevice,
        ViewDesc,
        pBuffer,
        bIsDefaultView
    },
    m_pD3D11View{pD3D11View}
// clang-format on
{
    if (*m_Desc.Name != 0)
    {
        HRESULT hr = m_pD3D11View->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen(m_Desc.Name)), m_Desc.Name);
        DEV_CHECK_ERR(SUCCEEDED(hr), "Failed to set buffer view name");
    }
}

IMPLEMENT_QUERY_INTERFACE(BufferViewD3D11Impl, IID_BufferViewD3D11, TBufferViewBase)

} // namespace Diligent
