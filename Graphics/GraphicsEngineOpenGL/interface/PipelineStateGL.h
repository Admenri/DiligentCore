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

#pragma once

/// \file
/// Definition of the Diligent::IPipelineStateGL interface

#include "../../GraphicsEngine/interface/PipelineState.h"

DILIGENT_BEGIN_NAMESPACE(Diligent)

// {80666BE3-318A-4403-AEE1-6E61A5B7A0F9}
static DILIGENT_CONSTEXPR INTERFACE_ID IID_PipelineStateGL =
    {0x80666be3, 0x318a, 0x4403, {0xae, 0xe1, 0x6e, 0x61, 0xa5, 0xb7, 0xa0, 0xf9}};

#define DILIGENT_INTERFACE_NAME IPipelineStateGL
#include "../../../Primitives/interface/DefineInterfaceHelperMacros.h"

#define IPipelineStateGLInclusiveMethods \
    IPipelineStateInclusiveMethods;      \
    IPipelineStateGLMethods PipelineStateGL

/// Exposes OpenGL-specific functionality of a pipeline state object.
DILIGENT_BEGIN_INTERFACE(IPipelineStateGL, IPipelineState)
{
    // clang-format off

    /// Returns OpenGL program handle for the specified shader stage.
    ///
    /// \param [in] Stage - Shader stage.
    /// \return OpenGL program handle for the specified shader stage.
    ///
    /// If device supports separable programs, the function
    /// returns the handle of the program for the specified
    /// shader stage. Otherwise, the function returns the handle
    /// of the program that contains all shaders, if Stage is
    /// one of the active shader stages, or zero otherwise.
    VIRTUAL GLuint METHOD(GetGLProgramHandle)(THIS_
                                              SHADER_TYPE Stage) CONST PURE;

    // clang-format on
};
DILIGENT_END_INTERFACE

#include "../../../Primitives/interface/UndefInterfaceHelperMacros.h"

#if DILIGENT_C_INTERFACE

// clang-format off

#    define IPipelineStateGL_GetGLProgramHandle(This, ...) CALL_IFACE_METHOD(PipelineStateGL, GetGLProgramHandle, This, __VA_ARGS__)

// clang-format on

#endif
DILIGENT_END_NAMESPACE // namespace Diligent
