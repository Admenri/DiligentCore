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

#include <limits>
#include <cstring>
#include <algorithm>

#include "VulkanErrors.hpp"
#include "VulkanUtilities/PhysicalDevice.hpp"

namespace VulkanUtilities
{

std::unique_ptr<PhysicalDevice> PhysicalDevice::Create(const CreateInfo& CI)
{
    return std::unique_ptr<PhysicalDevice>{new PhysicalDevice{CI}};
}

PhysicalDevice::PhysicalDevice(const CreateInfo& CI) :
    m_vkDevice{CI.vkDevice}
{
    VERIFY_EXPR(m_vkDevice != VK_NULL_HANDLE);

    vkGetPhysicalDeviceProperties(m_vkDevice, &m_Properties);
    vkGetPhysicalDeviceFeatures(m_vkDevice, &m_Features);
    vkGetPhysicalDeviceMemoryProperties(m_vkDevice, &m_MemoryProperties);
    uint32_t QueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkDevice, &QueueFamilyCount, nullptr);
    VERIFY_EXPR(QueueFamilyCount > 0);
    m_QueueFamilyProperties.resize(QueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_vkDevice, &QueueFamilyCount, m_QueueFamilyProperties.data());
    VERIFY_EXPR(QueueFamilyCount == m_QueueFamilyProperties.size());

    // Get list of supported extensions
    uint32_t ExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_vkDevice, nullptr, &ExtensionCount, nullptr);
    if (ExtensionCount > 0)
    {
        m_SupportedExtensions.resize(ExtensionCount);
        VkResult res = vkEnumerateDeviceExtensionProperties(m_vkDevice, nullptr, &ExtensionCount, m_SupportedExtensions.data());
        VERIFY_EXPR(res == VK_SUCCESS);
        (void)res;
        VERIFY_EXPR(ExtensionCount == m_SupportedExtensions.size());
    }

    if (CI.LogExtensions && !m_SupportedExtensions.empty())
    {
        LOG_INFO_MESSAGE("Extensions supported by device '", m_Properties.deviceName, "': ", PrintExtensionsList(m_SupportedExtensions, 3));
    }

    m_vkVersion = std::min(CI.Inst.GetVersion(), m_Properties.apiVersion);

    // remove patch version
    m_vkVersion &= ~VK_MAKE_VERSION(0, 0, VK_API_VERSION_PATCH(~0u));

#if DILIGENT_USE_VOLK
    if (CI.Inst.IsExtensionEnabled(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
        VkPhysicalDeviceFeatures2 Feats2{};
        Feats2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        void** NextFeat = &Feats2.pNext;

        VkPhysicalDeviceProperties2 Props2{};
        Props2.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        void** NextProp = &Props2.pNext;

        if (IsExtensionSupported(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.ShaderFloat16Int8;
            NextFeat  = &m_ExtFeatures.ShaderFloat16Int8.pNext;

            m_ExtFeatures.ShaderFloat16Int8.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
        }

        // VK_KHR_16bit_storage and VK_KHR_8bit_storage extensions require VK_KHR_storage_buffer_storage_class extension.
        if (IsExtensionSupported(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME))
        {
            if (IsExtensionSupported(VK_KHR_16BIT_STORAGE_EXTENSION_NAME))
            {
                *NextFeat = &m_ExtFeatures.Storage16Bit;
                NextFeat  = &m_ExtFeatures.Storage16Bit.pNext;

                m_ExtFeatures.Storage16Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
            }

            if (IsExtensionSupported(VK_KHR_8BIT_STORAGE_EXTENSION_NAME))
            {
                *NextFeat = &m_ExtFeatures.Storage8Bit;
                NextFeat  = &m_ExtFeatures.Storage8Bit.pNext;

                m_ExtFeatures.Storage8Bit.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
            }
        }

        // Get mesh shader features and properties.
        if (IsExtensionSupported(VK_EXT_MESH_SHADER_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.MeshShader;
            NextFeat  = &m_ExtFeatures.MeshShader.pNext;

            m_ExtFeatures.MeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

            *NextProp = &m_ExtProperties.MeshShader;
            NextProp  = &m_ExtProperties.MeshShader.pNext;

            m_ExtProperties.MeshShader.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
        }

        // Get acceleration structure features and properties.
        if (IsExtensionSupported(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.AccelStruct;
            NextFeat  = &m_ExtFeatures.AccelStruct.pNext;

            m_ExtFeatures.AccelStruct.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

            *NextProp = &m_ExtProperties.AccelStruct;
            NextProp  = &m_ExtProperties.AccelStruct.pNext;

            m_ExtProperties.AccelStruct.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
        }

        // Get ray tracing pipeline features and properties.
        if (IsExtensionSupported(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.RayTracingPipeline;
            NextFeat  = &m_ExtFeatures.RayTracingPipeline.pNext;

            m_ExtFeatures.RayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

            *NextProp = &m_ExtProperties.RayTracingPipeline;
            NextProp  = &m_ExtProperties.RayTracingPipeline.pNext;

            m_ExtProperties.RayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        }

        // Get inline ray tracing features.
        if (IsExtensionSupported(VK_KHR_RAY_QUERY_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.RayQuery;
            NextFeat  = &m_ExtFeatures.RayQuery.pNext;

            m_ExtFeatures.RayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        }

        // Additional extension that is required for ray tracing.
        if (IsExtensionSupported(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.BufferDeviceAddress;
            NextFeat  = &m_ExtFeatures.BufferDeviceAddress.pNext;

            m_ExtFeatures.BufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
        }

        // Additional extension that is required for ray tracing.
        if (IsExtensionSupported(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.DescriptorIndexing;
            NextFeat  = &m_ExtFeatures.DescriptorIndexing.pNext;

            m_ExtFeatures.DescriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;

            *NextProp = &m_ExtProperties.DescriptorIndexing;
            NextProp  = &m_ExtProperties.DescriptorIndexing.pNext;

            m_ExtProperties.DescriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES_EXT;
        }

        // Additional extension that is required for ray tracing shader.
        if (IsExtensionSupported(VK_KHR_SPIRV_1_4_EXTENSION_NAME))
            m_ExtFeatures.Spirv14 = true;

        // Some features require SPIRV 1.4 or 1.5 which was added to the Vulkan 1.2 core.
        if (m_vkVersion >= VK_API_VERSION_1_2)
        {
            m_ExtFeatures.Spirv14 = true;
            m_ExtFeatures.Spirv15 = true;
        }

        // Extension required for MoltenVk
        if (IsExtensionSupported(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.PortabilitySubset;
            NextFeat  = &m_ExtFeatures.PortabilitySubset.pNext;

            m_ExtFeatures.HasPortabilitySubset    = true;
            m_ExtFeatures.PortabilitySubset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;

            *NextProp = &m_ExtProperties.PortabilitySubset;
            NextProp  = &m_ExtProperties.PortabilitySubset.pNext;

            m_ExtProperties.PortabilitySubset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_PROPERTIES_KHR;
        }

        // Subgroup feature requires Vulkan 1.1 core.
        if (m_vkVersion >= VK_API_VERSION_1_1)
        {
            *NextProp = &m_ExtProperties.Subgroup;
            NextProp  = &m_ExtProperties.Subgroup.pNext;

            m_ExtFeatures.SubgroupOps      = true;
            m_ExtProperties.Subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        }

        if (IsExtensionSupported(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.VertexAttributeDivisor;
            NextFeat  = &m_ExtFeatures.VertexAttributeDivisor.pNext;

            m_ExtFeatures.VertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT;

            *NextProp = &m_ExtProperties.VertexAttributeDivisor;
            NextProp  = &m_ExtProperties.VertexAttributeDivisor.pNext;

            m_ExtProperties.VertexAttributeDivisor.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT;
        }

        if (IsExtensionSupported(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.TimelineSemaphore;
            NextFeat  = &m_ExtFeatures.TimelineSemaphore.pNext;

            m_ExtFeatures.TimelineSemaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;

            *NextProp = &m_ExtProperties.TimelineSemaphore;
            NextProp  = &m_ExtProperties.TimelineSemaphore.pNext;

            m_ExtProperties.TimelineSemaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;
        }

        if (IsExtensionSupported(VK_KHR_MULTIVIEW_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.Multiview;
            NextFeat  = &m_ExtFeatures.Multiview.pNext;

            m_ExtFeatures.Multiview.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR;

            *NextProp = &m_ExtProperties.Multiview;
            NextProp  = &m_ExtProperties.Multiview.pNext;

            m_ExtProperties.Multiview.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES_KHR;
        }

        if (IsExtensionSupported(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME))
        {
            m_ExtFeatures.RenderPass2 = true;
        }

        if (IsExtensionSupported(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.ShadingRate;
            NextFeat  = &m_ExtFeatures.ShadingRate.pNext;

            m_ExtFeatures.ShadingRate.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;

            *NextProp = &m_ExtProperties.ShadingRate;
            NextProp  = &m_ExtProperties.ShadingRate.pNext;

            m_ExtProperties.ShadingRate.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
        }

        if (IsExtensionSupported(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.FragmentDensityMap;
            NextFeat  = &m_ExtFeatures.FragmentDensityMap.pNext;

            m_ExtFeatures.FragmentDensityMap.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT;

            *NextProp = &m_ExtProperties.FragmentDensityMap;
            NextProp  = &m_ExtProperties.FragmentDensityMap.pNext;

            m_ExtProperties.FragmentDensityMap.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT;
        }

        if (IsExtensionSupported(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.HostQueryReset;
            NextFeat  = &m_ExtFeatures.HostQueryReset.pNext;

            m_ExtFeatures.HostQueryReset.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
        }

        if (IsExtensionSupported(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME))
        {
            m_ExtFeatures.DrawIndirectCount = true;
        }

        if (IsExtensionSupported(VK_KHR_MAINTENANCE3_EXTENSION_NAME))
        {
            *NextProp = &m_ExtProperties.Maintenance3;
            NextProp  = &m_ExtProperties.Maintenance3.pNext;

            m_ExtProperties.Maintenance3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
        }

        if (IsExtensionSupported(VK_EXT_MULTI_DRAW_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.MultiDraw;
            NextFeat  = &m_ExtFeatures.MultiDraw.pNext;

            m_ExtFeatures.MultiDraw.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_FEATURES_EXT;

            *NextFeat = &m_ExtFeatures.ShaderDrawParameters;
            NextFeat  = &m_ExtFeatures.ShaderDrawParameters.pNext;

            m_ExtFeatures.ShaderDrawParameters.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETER_FEATURES;

            *NextProp = &m_ExtProperties.MultiDraw;
            NextProp  = &m_ExtProperties.MultiDraw.pNext;

            m_ExtProperties.MultiDraw.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTI_DRAW_PROPERTIES_EXT;
        }

        if (IsExtensionSupported(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME))
        {
            *NextFeat = &m_ExtFeatures.DynamicRendering;
            NextFeat  = &m_ExtFeatures.DynamicRendering.pNext;

            m_ExtFeatures.DynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        }

        const bool HostImageCopySupported = IsExtensionSupported(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME);
        if (HostImageCopySupported)
        {
            *NextFeat = &m_ExtFeatures.HostImageCopy;
            NextFeat  = &m_ExtFeatures.HostImageCopy.pNext;

            m_ExtFeatures.HostImageCopy.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT;

            *NextProp = &m_ExtProperties.HostImageCopy;
            NextProp  = &m_ExtProperties.HostImageCopy.pNext;

            m_ExtProperties.HostImageCopy.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES_EXT;
        }

        // make sure that last pNext is null
        *NextFeat = nullptr;
        *NextProp = nullptr;

        // Initialize device extension features by current physical device features.
        // Some flags may not be supported by hardware.
        vkGetPhysicalDeviceFeatures2KHR(m_vkDevice, &Feats2);
        vkGetPhysicalDeviceProperties2KHR(m_vkDevice, &Props2);

        if (HostImageCopySupported)
        {
            m_ExtProperties.HostImageCopy.pNext = nullptr;
            if (m_ExtProperties.HostImageCopy.copySrcLayoutCount + m_ExtProperties.HostImageCopy.copyDstLayoutCount > 0)
            {
                m_ExtProperties.HostImageCopyLayouts          = std::make_unique<VkImageLayout[]>(m_ExtProperties.HostImageCopy.copySrcLayoutCount + m_ExtProperties.HostImageCopy.copyDstLayoutCount);
                m_ExtProperties.HostImageCopy.pCopySrcLayouts = m_ExtProperties.HostImageCopy.copySrcLayoutCount > 0 ? &m_ExtProperties.HostImageCopyLayouts[0] : nullptr;
                m_ExtProperties.HostImageCopy.pCopyDstLayouts = m_ExtProperties.HostImageCopy.copyDstLayoutCount > 0 ? &m_ExtProperties.HostImageCopyLayouts[m_ExtProperties.HostImageCopy.copySrcLayoutCount] : nullptr;
            }
            Props2.pNext = &m_ExtProperties.HostImageCopy;
            vkGetPhysicalDeviceProperties2KHR(m_vkDevice, &Props2);
        }

        // Check texture formats
        if (m_ExtFeatures.ShadingRate.attachmentFragmentShadingRate != VK_FALSE)
        {
            // For compatibility with D3D12, shading rate texture must support R8_UINT format
            VkFormatProperties FmtProps{};
            vkGetPhysicalDeviceFormatProperties(m_vkDevice, VK_FORMAT_R8_UINT, &FmtProps);

            // Disable feature if image format is not supported
            if (!(FmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR))
            {
                m_ExtFeatures.ShadingRate   = {};
                m_ExtProperties.ShadingRate = {};
            }
        }
        if (m_ExtFeatures.FragmentDensityMap.fragmentDensityMap != VK_FALSE)
        {
            VkFormatProperties FmtProps{};
            vkGetPhysicalDeviceFormatProperties(m_vkDevice, VK_FORMAT_R8G8_UNORM, &FmtProps);

            // Disable feature if image format is not supported
            if (!(FmtProps.optimalTilingFeatures & VK_FORMAT_FEATURE_FRAGMENT_DENSITY_MAP_BIT_EXT))
            {
                m_ExtFeatures.FragmentDensityMap    = {};
                m_ExtProperties.FragmentDensityMap  = {};
                m_ExtFeatures.FragmentDensityMap2   = {};
                m_ExtProperties.FragmentDensityMap2 = {};
            }
        }
    }
#endif // DILIGENT_USE_VOLK
}

HardwareQueueIndex PhysicalDevice::FindQueueFamily(VkQueueFlags QueueFlags) const
{
    // All commands that are allowed on a queue that supports transfer operations are also allowed on
    // a queue that supports either graphics or compute operations. Thus, if the capabilities of a queue
    // family include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT, then reporting the VK_QUEUE_TRANSFER_BIT
    // capability separately for that queue family is optional (4.1).
    VkQueueFlags QueueFlagsOpt = QueueFlags;
    if (QueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
    {
        QueueFlags &= ~VK_QUEUE_TRANSFER_BIT;
        QueueFlagsOpt = QueueFlags | VK_QUEUE_TRANSFER_BIT;
    }

    static constexpr uint32_t InvalidFamilyInd = std::numeric_limits<uint32_t>::max();
    uint32_t                  FamilyInd        = InvalidFamilyInd;

    for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); ++i)
    {
        // First try to find a queue, for which the flags match exactly
        // (i.e. dedicated compute or transfer queue)
        const VkQueueFamilyProperties& Props = m_QueueFamilyProperties[i];
        if (Props.queueFlags == QueueFlags ||
            Props.queueFlags == QueueFlagsOpt)
        {
            FamilyInd = i;
            break;
        }
    }

    if (FamilyInd == InvalidFamilyInd)
    {
        for (uint32_t i = 0; i < m_QueueFamilyProperties.size(); ++i)
        {
            // Try to find a queue for which all requested flags are set
            const VkQueueFamilyProperties& Props = m_QueueFamilyProperties[i];
            // Check only QueueFlags as VK_QUEUE_TRANSFER_BIT is
            // optional for graphics and/or compute queues
            if ((Props.queueFlags & QueueFlags) == QueueFlags)
            {
                FamilyInd = i;
                break;
            }
        }
    }

    if (FamilyInd != InvalidFamilyInd)
    {
        if (QueueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
#ifdef DILIGENT_DEBUG
            const VkQueueFamilyProperties& Props = m_QueueFamilyProperties[FamilyInd];
            // Queues supporting graphics and/or compute operations must report (1,1,1)
            // in minImageTransferGranularity, meaning that there are no additional restrictions
            // on the granularity of image transfer operations for these queues (4.1).
            VERIFY_EXPR(Props.minImageTransferGranularity.width == 1 &&
                        Props.minImageTransferGranularity.height == 1 &&
                        Props.minImageTransferGranularity.depth == 1);
#endif
        }
    }
    else
    {
        LOG_ERROR_AND_THROW("Failed to find suitable queue family");
    }

    return HardwareQueueIndex{FamilyInd};
}

bool PhysicalDevice::IsExtensionSupported(const char* ExtensionName) const
{
    for (const VkExtensionProperties& Extension : m_SupportedExtensions)
        if (strcmp(Extension.extensionName, ExtensionName) == 0)
            return true;

    return false;
}

bool PhysicalDevice::CheckPresentSupport(HardwareQueueIndex queueFamilyIndex, VkSurfaceKHR VkSurface) const
{
    VkBool32 PresentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_vkDevice, queueFamilyIndex, VkSurface, &PresentSupport);
    return PresentSupport == VK_TRUE;
}


// This function is used to find a device memory type that supports all the property flags we request
// Params:
// * memoryTypeBitsRequirement  -  a bitmask that contains one bit set for every supported memory type for
//                                 the resource. Bit i is set if the memory type i in the
//                                 VkPhysicalDeviceMemoryProperties structure for the physical device is
//                                 supported for the resource.
// * requiredProperties   -  required memory properties (device local, host visible, etc.)
uint32_t PhysicalDevice::GetMemoryTypeIndex(uint32_t              memoryTypeBitsRequirement,
                                            VkMemoryPropertyFlags requiredProperties) const
{
    // Iterate over all memory types available for the device
    // For each pair of elements X and Y returned in memoryTypes, X must be placed at a lower index position than Y if:
    //   * either the set of bit flags of X is a strict subset of the set of bit flags of Y.
    //   * or the propertyFlags members of X and Y are equal, and X belongs to a memory heap with greater performance

    for (uint32_t memoryIndex = 0; memoryIndex < m_MemoryProperties.memoryTypeCount; memoryIndex++)
    {
        // Each memory type returned by vkGetPhysicalDeviceMemoryProperties must have its propertyFlags set
        // to one of the following values:
        // * 0
        // * HOST_VISIBLE_BIT | HOST_COHERENT_BIT
        // * HOST_VISIBLE_BIT | HOST_CACHED_BIT
        // * HOST_VISIBLE_BIT | HOST_CACHED_BIT | HOST_COHERENT_BIT
        // * DEVICE_LOCAL_BIT
        // * DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_COHERENT_BIT
        // * DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_CACHED_BIT
        // * DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_CACHED_BIT | HOST_COHERENT_BIT
        // * DEVICE_LOCAL_BIT | LAZILY_ALLOCATED_BIT
        //
        // There must be at least one memory type with both the HOST_VISIBLE_BIT and HOST_COHERENT_BIT bits set
        // There must be at least one memory type with the DEVICE_LOCAL_BIT bit set

        const uint32_t memoryTypeBit        = (1 << memoryIndex);
        const bool     isRequiredMemoryType = (memoryTypeBitsRequirement & memoryTypeBit) != 0;
        if (isRequiredMemoryType)
        {
            const VkMemoryPropertyFlags properties            = m_MemoryProperties.memoryTypes[memoryIndex].propertyFlags;
            const bool                  hasRequiredProperties = (properties & requiredProperties) == requiredProperties;

            if (hasRequiredProperties)
                return memoryIndex;
        }
    }
    return InvalidMemoryTypeIndex;
}

VkFormatProperties PhysicalDevice::GetPhysicalDeviceFormatProperties(VkFormat imageFormat, VkFormatProperties3* Properties3) const
{
    if (Properties3 != nullptr)
    {
        *Properties3       = {};
        Properties3->sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR;

        VkFormatProperties2 VkFormatProps2{};
        VkFormatProps2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        VkFormatProps2.pNext = Properties3;

        vkGetPhysicalDeviceFormatProperties2(m_vkDevice, imageFormat, &VkFormatProps2);

        return VkFormatProps2.formatProperties;
    }
    else
    {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(m_vkDevice, imageFormat, &formatProperties);
        return formatProperties;
    }
}

bool PhysicalDevice::IsUMA() const
{
    return m_MemoryProperties.memoryHeapCount == 1;
}

} // namespace VulkanUtilities
