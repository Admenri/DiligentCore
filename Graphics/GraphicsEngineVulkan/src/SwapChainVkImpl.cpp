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
#include "SwapChainVkImpl.hpp"
#include "RenderDeviceVkImpl.hpp"
#include "DeviceContextVkImpl.hpp"
#include "TextureVkImpl.hpp"
#include "VulkanTypeConversions.hpp"
#include "EngineMemory.h"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

SwapChainVkImpl::SwapChainVkImpl(IReferenceCounters*  pRefCounters,
                                 const SwapChainDesc& SCDesc,
                                 RenderDeviceVkImpl*  pRenderDeviceVk,
                                 DeviceContextVkImpl* pDeviceContextVk,
                                 const NativeWindow&  Window) :
    // clang-format off
    TSwapChainBase               {pRefCounters, pRenderDeviceVk, pDeviceContextVk, SCDesc},
    m_Window                     {Window},
    m_Instance             {pRenderDeviceVk->GetInstance()},
    m_DesiredBufferCount         {SCDesc.BufferCount},
    m_pBackBufferRTV             (STD_ALLOCATOR_RAW_MEM(RefCntAutoPtr<ITextureView>, GetRawAllocator(), "Allocator for vector<RefCntAutoPtr<ITextureView>>")),
    m_SwapChainImagesInitialized (STD_ALLOCATOR_RAW_MEM(bool, GetRawAllocator(), "Allocator for vector<bool>"))
// clang-format on
{
    CreateSurface();
    CreateVulkanSwapChain();
    InitBuffersAndViews();

    AcquireNextImage(pDeviceContextVk);
    // Note that the image may be immediately out of date.
    // https://github.com/DiligentGraphics/DiligentCore/issues/632

    FenceDesc FenceCI;
    FenceCI.Name = "Swap chain frame complete fence";
    pRenderDeviceVk->CreateFence(FenceCI, &m_FrameCompleteFence);
}

void SwapChainVkImpl::CreateSurface()
{
    if (m_VkSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance->GetVkInstance(), m_VkSurface, NULL);
        m_VkSurface = VK_NULL_HANDLE;
    }

    // Create OS-specific surface
    VkResult err = VK_ERROR_INITIALIZATION_FAILED;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (m_Window.hWnd != NULL)
    {
        VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
        surfaceCreateInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.hinstance = GetModuleHandle(NULL);
        surfaceCreateInfo.hwnd      = (HWND)m_Window.hWnd;

        err = vkCreateWin32SurfaceKHR(m_Instance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
    }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    if (m_Window.pAWindow != nullptr)
    {
        VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{};
        surfaceCreateInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.window = (ANativeWindow*)m_Window.pAWindow;

        err = vkCreateAndroidSurfaceKHR(m_Instance->GetVkInstance(), &surfaceCreateInfo, NULL, &m_VkSurface);
    }
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    if (void* pLayer = m_Window.GetLayer())
    {
        VkMetalSurfaceCreateInfoEXT surfaceCreateInfo{};
        surfaceCreateInfo.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        surfaceCreateInfo.pLayer = pLayer;

        err = vkCreateMetalSurfaceEXT(m_Instance->GetVkInstance(), &surfaceCreateInfo, NULL, &m_VkSurface);
    }
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    if (m_Window.pDisplay != nullptr)
    {
        VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo{};
        surfaceCreateInfo.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.display = reinterpret_cast<struct wl_display*>(m_Window.pDisplay);
        surfaceCreateInfo.Surface = reinterpret_cast<struct wl_surface*>(nullptr);

        err = vkCreateWaylandSurfaceKHR(m_Instance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
    }
#elif defined(VK_USE_PLATFORM_XCB_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR)

#    if defined(VK_USE_PLATFORM_XCB_KHR)
    if (m_Window.pXCBConnection != nullptr && m_Window.WindowId != 0)
    {
        VkXcbSurfaceCreateInfoKHR surfaceCreateInfo{};
        surfaceCreateInfo.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.connection = reinterpret_cast<xcb_connection_t*>(m_Window.pXCBConnection);
        surfaceCreateInfo.window     = m_Window.WindowId;

        err = vkCreateXcbSurfaceKHR(m_Instance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
    }
#    endif

#    if defined(VK_USE_PLATFORM_XLIB_KHR)
    if ((m_Window.pDisplay != nullptr && m_Window.WindowId != 0) && m_VkSurface == VK_NULL_HANDLE)
    {
        VkXlibSurfaceCreateInfoKHR surfaceCreateInfo{};
        surfaceCreateInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.dpy    = reinterpret_cast<Display*>(m_Window.pDisplay);
        surfaceCreateInfo.window = m_Window.WindowId;

        err = vkCreateXlibSurfaceKHR(m_Instance->GetVkInstance(), &surfaceCreateInfo, nullptr, &m_VkSurface);
    }
#    endif

#endif

    CHECK_VK_ERROR_AND_THROW(err, "Failed to create OS-specific surface");

    if (RefCntAutoPtr<IDeviceContext> pContext = m_wpDeviceContext.Lock())
    {
        RenderDeviceVkImpl*                    pRenderDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
        const VulkanUtilities::PhysicalDevice& PhysicalDevice  = pRenderDeviceVk->GetPhysicalDevice();
        const ICommandQueueVk&                 CmdQueueVK      = pRenderDeviceVk->GetCommandQueue(pContext.RawPtr<DeviceContextVkImpl>()->GetCommandQueueId());
        HardwareQueueIndex                     QueueFamilyIndex{CmdQueueVK.GetQueueFamilyIndex()};
        if (!PhysicalDevice.CheckPresentSupport(QueueFamilyIndex, m_VkSurface))
        {
            LOG_ERROR_AND_THROW("Selected physical device does not support present capability.\n"
                                "There could be few ways to mitigate this problem. One is to try to find another queue that supports present, but does not support graphics and compute capabilities."
                                "Another way is to find another physical device that exposes queue family that supports present and graphics capability. Neither approach is currently implemented in Diligent Engine.");
        }
    }
    else
    {
        DEV_ERROR("Immediate context has been released");
    }
}

void SwapChainVkImpl::CreateVulkanSwapChain()
{
    RenderDeviceVkImpl*                    pRenderDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    const VulkanUtilities::PhysicalDevice& PhysicalDevice  = pRenderDeviceVk->GetPhysicalDevice();
    VkPhysicalDevice                       vkDeviceHandle  = PhysicalDevice.GetVkDeviceHandle();
    // Get the list of VkFormats that are supported:
    uint32_t formatCount = 0;

    VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(vkDeviceHandle, m_VkSurface, &formatCount, NULL);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query number of supported formats");
    VERIFY_EXPR(formatCount > 0);
    std::vector<VkSurfaceFormatKHR> SupportedFormats(formatCount);
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(vkDeviceHandle, m_VkSurface, &formatCount, SupportedFormats.data());
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query supported format properties");
    VERIFY_EXPR(formatCount == SupportedFormats.size());
    m_VkColorFormat = TexFormatToVkFormat(m_SwapChainDesc.ColorBufferFormat);

    VkColorSpaceKHR ColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    if (formatCount == 1 && SupportedFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
        // the surface has no preferred format.  Otherwise, at least one
        // supported format will be returned.

        // Do nothing
    }
    else
    {
        bool FmtFound = false;
        for (const VkSurfaceFormatKHR& SrfFmt : SupportedFormats)
        {
            if (SrfFmt.format == m_VkColorFormat)
            {
                FmtFound   = true;
                ColorSpace = SrfFmt.colorSpace;
                break;
            }
        }
        if (!FmtFound)
        {
            VkFormat VkReplacementColorFormat = VK_FORMAT_UNDEFINED;
            switch (m_VkColorFormat)
            {
                    // clang-format off
                case VK_FORMAT_R8G8B8A8_UNORM: VkReplacementColorFormat = VK_FORMAT_B8G8R8A8_UNORM; break;
                case VK_FORMAT_B8G8R8A8_UNORM: VkReplacementColorFormat = VK_FORMAT_R8G8B8A8_UNORM; break;
                case VK_FORMAT_B8G8R8A8_SRGB:  VkReplacementColorFormat = VK_FORMAT_R8G8B8A8_SRGB;  break;
                case VK_FORMAT_R8G8B8A8_SRGB:  VkReplacementColorFormat = VK_FORMAT_B8G8R8A8_SRGB;  break;
                default: VkReplacementColorFormat = VK_FORMAT_UNDEFINED;
                    // clang-format on
            }

            bool ReplacementFmtFound = false;
            for (const VkSurfaceFormatKHR& SrfFmt : SupportedFormats)
            {
                if (SrfFmt.format == VkReplacementColorFormat)
                {
                    ReplacementFmtFound = true;
                    ColorSpace          = SrfFmt.colorSpace;
                    break;
                }
            }

            if (ReplacementFmtFound)
            {
                m_VkColorFormat                     = VkReplacementColorFormat;
                TEXTURE_FORMAT NewColorBufferFormat = VkFormatToTexFormat(VkReplacementColorFormat);
                LOG_INFO_MESSAGE("Requested color buffer format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name, " is not supported by the surface and will be replaced with ", GetTextureFormatAttribs(NewColorBufferFormat).Name);
                m_SwapChainDesc.ColorBufferFormat = NewColorBufferFormat;
            }
            else
            {
                LOG_WARNING_MESSAGE("Requested color buffer format ", GetTextureFormatAttribs(m_SwapChainDesc.ColorBufferFormat).Name, "is not supported by the surface");
            }
        }
    }

    VkSurfaceCapabilitiesKHR surfCapabilities = {};

    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDeviceHandle, m_VkSurface, &surfCapabilities);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query physical device surface capabilities");

    uint32_t presentModeCount = 0;

    err = vkGetPhysicalDeviceSurfacePresentModesKHR(vkDeviceHandle, m_VkSurface, &presentModeCount, NULL);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query surface present mode count");
    VERIFY_EXPR(presentModeCount > 0);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(vkDeviceHandle, m_VkSurface, &presentModeCount, presentModes.data());
    CHECK_VK_ERROR_AND_THROW(err, "Failed to query surface present modes");
    VERIFY_EXPR(presentModeCount == presentModes.size());


    VkSurfaceTransformFlagBitsKHR vkPreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if (m_DesiredPreTransform != SURFACE_TRANSFORM_OPTIMAL)
    {
        vkPreTransform = SurfaceTransformToVkSurfaceTransformFlag(m_DesiredPreTransform);
        if ((surfCapabilities.supportedTransforms & vkPreTransform) != 0)
        {
            m_SwapChainDesc.PreTransform = m_DesiredPreTransform;
        }
        else
        {
            LOG_WARNING_MESSAGE(GetSurfaceTransformString(m_DesiredPreTransform),
                                " is not supported by the presentation engine. Optimal surface transform will be used instead."
                                " Query the swap chain description to get the actual surface transform.");
            m_DesiredPreTransform = SURFACE_TRANSFORM_OPTIMAL;
        }
    }

    if (m_DesiredPreTransform == SURFACE_TRANSFORM_OPTIMAL)
    {
        // Use current surface transform to avoid extra cost of presenting the image.
        // If preTransform does not match the currentTransform value returned by vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
        // the presentation engine will transform the image content as part of the presentation operation.
        // https://android-developers.googleblog.com/2020/02/handling-device-orientation-efficiently.html
        // https://community.arm.com/developer/tools-software/graphics/b/blog/posts/appropriate-use-of-surface-rotation
        vkPreTransform               = surfCapabilities.currentTransform;
        m_SwapChainDesc.PreTransform = VkSurfaceTransformFlagToSurfaceTransform(vkPreTransform);
        LOG_INFO_MESSAGE("Using ", GetSurfaceTransformString(m_SwapChainDesc.PreTransform), " swap chain pretransform");
    }

    VkExtent2D swapchainExtent = {};
    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF && m_SwapChainDesc.Width != 0 && m_SwapChainDesc.Height != 0)
    {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width  = std::min(std::max(m_SwapChainDesc.Width, surfCapabilities.minImageExtent.width), surfCapabilities.maxImageExtent.width);
        swapchainExtent.height = std::min(std::max(m_SwapChainDesc.Height, surfCapabilities.minImageExtent.height), surfCapabilities.maxImageExtent.height);
    }
    else
    {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent;
    }

#if PLATFORM_ANDROID
    // On Android, vkGetPhysicalDeviceSurfaceCapabilitiesKHR is not reliable and starts reporting incorrect
    // dimensions after few rotations. To alleviate the problem, we store the surface extent corresponding to
    // identity rotation.
    // https://android-developers.googleblog.com/2020/02/handling-device-orientation-efficiently.html
    if (m_SurfaceIdentityExtent.width == 0 || m_SurfaceIdentityExtent.height == 0)
    {
        m_SurfaceIdentityExtent = surfCapabilities.currentExtent;
        constexpr auto Rotate90TransformFlags =
            VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
            VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR |
            VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR |
            VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;
        if ((surfCapabilities.currentTransform & Rotate90TransformFlags) != 0)
            std::swap(m_SurfaceIdentityExtent.width, m_SurfaceIdentityExtent.height);
    }

    if (m_DesiredPreTransform == SURFACE_TRANSFORM_OPTIMAL)
    {
        swapchainExtent = m_SurfaceIdentityExtent;
    }
    m_CurrentSurfaceTransform = surfCapabilities.currentTransform;
#endif

    swapchainExtent.width  = std::max(swapchainExtent.width, 1u);
    swapchainExtent.height = std::max(swapchainExtent.height, 1u);
    m_SwapChainDesc.Width  = swapchainExtent.width;
    m_SwapChainDesc.Height = swapchainExtent.height;

    // The FIFO present mode is guaranteed by the spec to always be supported.
    VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
        std::vector<VkPresentModeKHR> PreferredPresentModes;
        if (m_VSyncEnabled)
        {
            // FIFO relaxed waits for the next VSync, but if the frame is late,
            // it still shows it even if VSync has already passed, which may
            // result in tearing.
            PreferredPresentModes.push_back(VK_PRESENT_MODE_FIFO_RELAXED_KHR);
            PreferredPresentModes.push_back(VK_PRESENT_MODE_FIFO_KHR);
        }
        else
        {
            // Mailbox is the lowest latency non-tearing presentation mode.
            PreferredPresentModes.push_back(VK_PRESENT_MODE_MAILBOX_KHR);
            PreferredPresentModes.push_back(VK_PRESENT_MODE_IMMEDIATE_KHR);
            PreferredPresentModes.push_back(VK_PRESENT_MODE_FIFO_KHR);
        }

        for (VkPresentModeKHR PreferredMode : PreferredPresentModes)
        {
            if (std::find(presentModes.begin(), presentModes.end(), PreferredMode) != presentModes.end())
            {
                PresentMode = PreferredMode;
                break;
            }
        }

        const char* PresentModeName = nullptr;
#define PRESENT_MODE_CASE(Mode) \
    case Mode: PresentModeName = #Mode; break;
        switch (PresentMode)
        {
            PRESENT_MODE_CASE(VK_PRESENT_MODE_IMMEDIATE_KHR)
            PRESENT_MODE_CASE(VK_PRESENT_MODE_MAILBOX_KHR)
            PRESENT_MODE_CASE(VK_PRESENT_MODE_FIFO_KHR)
            PRESENT_MODE_CASE(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            PRESENT_MODE_CASE(VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
            PRESENT_MODE_CASE(VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)
            default: PresentModeName = "<UNKNOWN>";
        }
#undef PRESENT_MODE_CASE
        LOG_INFO_MESSAGE("Using ", PresentModeName, " swap chain present mode");
    }

    // Determine the number of VkImage's to use in the swap chain.
    // We need to acquire only 1 presentable image at at time.
    // Asking for minImageCount images ensures that we can acquire
    // 1 presentable image as long as we present it before attempting
    // to acquire another.
    if (m_DesiredBufferCount < surfCapabilities.minImageCount)
    {
        LOG_INFO_MESSAGE("Desired back buffer count (", m_DesiredBufferCount, ") is smaller than the minimal image count supported for this surface (", surfCapabilities.minImageCount, "). Resetting to ", surfCapabilities.minImageCount);
        m_DesiredBufferCount = surfCapabilities.minImageCount;
    }
    if (surfCapabilities.maxImageCount != 0 && m_DesiredBufferCount > surfCapabilities.maxImageCount)
    {
        LOG_INFO_MESSAGE("Desired back buffer count (", m_DesiredBufferCount, ") is greater than the maximal image count supported for this surface (", surfCapabilities.maxImageCount, "). Resetting to ", surfCapabilities.maxImageCount);
        m_DesiredBufferCount = surfCapabilities.maxImageCount;
    }
    // We must use m_DesiredBufferCount instead of m_SwapChainDesc.BufferCount, because Vulkan on Android
    // may decide to always add extra buffers, causing infinite growth of the swap chain when it is recreated:
    //                          m_SwapChainDesc.BufferCount
    // CreateVulkanSwapChain()          2 -> 4
    // CreateVulkanSwapChain()          4 -> 6
    // CreateVulkanSwapChain()          6 -> 8
    uint32_t desiredNumberOfSwapChainImages = m_DesiredBufferCount;

    // Find a supported composite alpha mode - one of these is guaranteed to be set
    VkCompositeAlphaFlagBitsKHR compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[4] = //
        {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
    for (uint32_t i = 0; i < _countof(compositeAlphaFlags); i++)
    {
        if (surfCapabilities.supportedCompositeAlpha & compositeAlphaFlags[i])
        {
            compositeAlpha = compositeAlphaFlags[i];
            break;
        }
    }

    VkSwapchainKHR oldSwapchain = m_VkSwapChain;
    m_VkSwapChain               = VK_NULL_HANDLE;

    VkSwapchainCreateInfoKHR swapchain_ci = {};

    swapchain_ci.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.pNext              = NULL;
    swapchain_ci.surface            = m_VkSurface;
    swapchain_ci.minImageCount      = desiredNumberOfSwapChainImages;
    swapchain_ci.imageFormat        = m_VkColorFormat;
    swapchain_ci.imageExtent.width  = swapchainExtent.width;
    swapchain_ci.imageExtent.height = swapchainExtent.height;
    swapchain_ci.preTransform       = vkPreTransform;
    swapchain_ci.compositeAlpha     = compositeAlpha;
    swapchain_ci.imageArrayLayers   = 1;
    swapchain_ci.presentMode        = PresentMode;
    swapchain_ci.oldSwapchain       = oldSwapchain;
    swapchain_ci.clipped            = VK_TRUE;
    swapchain_ci.imageColorSpace    = ColorSpace;

    DEV_CHECK_ERR(m_SwapChainDesc.Usage != 0, "No swap chain usage flags defined");
    static_assert(SWAP_CHAIN_USAGE_LAST == 8, "Please update this function to handle the new swapchain usage");
    if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_RENDER_TARGET)
        swapchain_ci.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_SHADER_RESOURCE)
        swapchain_ci.imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_INPUT_ATTACHMENT)
        swapchain_ci.imageUsage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    if (m_SwapChainDesc.Usage & SWAP_CHAIN_USAGE_COPY_SOURCE)
        swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    // vkCmdClearColorImage() command requires the image to use VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL layout
    // that requires  VK_IMAGE_USAGE_TRANSFER_DST_BIT to be set
    swapchain_ci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.pQueueFamilyIndices   = NULL;
    //uint32_t queueFamilyIndices[] = { (uint32_t)info.graphics_queue_family_index, (uint32_t)info.present_queue_family_index };
    //if (info.graphics_queue_family_index != info.present_queue_family_index) {
    //    // If the graphics and present queues are from different queue families,
    //    // we either have to explicitly transfer ownership of images between
    //    // the queues, or we have to create the swapchain with imageSharingMode
    //    // as VK_SHARING_MODE_CONCURRENT
    //    swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    //    swapchain_ci.queueFamilyIndexCount = 2;
    //    swapchain_ci.pQueueFamilyIndices = queueFamilyIndices;
    //}

    const VulkanUtilities::LogicalDevice& LogicalDevice = pRenderDeviceVk->GetLogicalDevice();
    VkDevice                              vkDevice      = pRenderDeviceVk->GetVkDevice();

    err = vkCreateSwapchainKHR(vkDevice, &swapchain_ci, NULL, &m_VkSwapChain);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to create Vulkan swapchain");

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vkDevice, oldSwapchain, NULL);
        oldSwapchain = VK_NULL_HANDLE;
    }

    uint32_t swapchainImageCount = 0;

    err = vkGetSwapchainImagesKHR(vkDevice, m_VkSwapChain, &swapchainImageCount, NULL);
    CHECK_VK_ERROR_AND_THROW(err, "Failed to request swap chain image count");
    VERIFY_EXPR(swapchainImageCount > 0);
    if (swapchainImageCount != m_SwapChainDesc.BufferCount)
    {
        LOG_INFO_MESSAGE("Created swap chain with ", swapchainImageCount,
                         " images vs ", m_SwapChainDesc.BufferCount, " requested.");
        m_SwapChainDesc.BufferCount = swapchainImageCount;
    }

    m_ImageAcquiredSemaphores.resize(swapchainImageCount);
    m_DrawCompleteSemaphores.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkSemaphoreCreateInfo SemaphoreCI = {};

        SemaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        SemaphoreCI.pNext = nullptr;
        SemaphoreCI.flags = 0; // reserved for future use

        {
            std::stringstream ss;
            ss << "Swap chain image acquired semaphore " << i;
            const std::string                 Name      = ss.str();
            VulkanUtilities::SemaphoreWrapper Semaphore = LogicalDevice.CreateSemaphore(SemaphoreCI, Name.c_str());
            ManagedSemaphore::Create(pRenderDeviceVk, std::move(Semaphore), Name.c_str(), &m_ImageAcquiredSemaphores[i]);
        }

        {
            std::stringstream ss;
            ss << "Swap chain draw complete semaphore " << i;
            const std::string                 Name      = ss.str();
            VulkanUtilities::SemaphoreWrapper Semaphore = LogicalDevice.CreateSemaphore(SemaphoreCI, Name.c_str());
            ManagedSemaphore::Create(pRenderDeviceVk, std::move(Semaphore), Name.c_str(), &m_DrawCompleteSemaphores[i]);
        }
    }
}

SwapChainVkImpl::~SwapChainVkImpl()
{
    if (m_VkSwapChain != VK_NULL_HANDLE)
    {
        RefCntAutoPtr<IDeviceContext> pDeviceContext  = m_wpDeviceContext.Lock();
        DeviceContextVkImpl*          pImmediateCtxVk = pDeviceContext.RawPtr<DeviceContextVkImpl>();
        ReleaseSwapChainResources(pImmediateCtxVk, /*DestroyVkSwapChain=*/true);
        VERIFY_EXPR(m_VkSwapChain == VK_NULL_HANDLE);
    }

    if (m_VkSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Instance->GetVkInstance(), m_VkSurface, NULL);
    }
}

void SwapChainVkImpl::InitBuffersAndViews()
{
    RenderDeviceVkImpl* pDeviceVkImpl   = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    VkDevice            LogicalVkDevice = pDeviceVkImpl->GetVkDevice();

#ifdef DILIGENT_DEBUG
    {
        uint32_t swapchainImageCount = 0;
        VkResult err                 = vkGetSwapchainImagesKHR(LogicalVkDevice, m_VkSwapChain, &swapchainImageCount, NULL);
        VERIFY_EXPR(err == VK_SUCCESS);
        VERIFY(swapchainImageCount == m_SwapChainDesc.BufferCount, "Unexpected swap chain buffer count");
    }
#endif

    m_pBackBufferRTV.resize(m_SwapChainDesc.BufferCount);
    m_SwapChainImagesInitialized.resize(m_pBackBufferRTV.size(), false);

    uint32_t             swapchainImageCount = m_SwapChainDesc.BufferCount;
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    VkResult             err = vkGetSwapchainImagesKHR(LogicalVkDevice, m_VkSwapChain, &swapchainImageCount, swapchainImages.data());
    CHECK_VK_ERROR_AND_THROW(err, "Failed to get swap chain images");
    VERIFY_EXPR(swapchainImageCount == swapchainImages.size());

    for (uint32_t i = 0; i < swapchainImageCount; i++)
    {
        TextureDesc       BackBufferDesc;
        std::stringstream name_ss;
        name_ss << "Main back buffer " << i;
        const std::string name   = name_ss.str();
        BackBufferDesc.Name      = name.c_str();
        BackBufferDesc.Type      = RESOURCE_DIM_TEX_2D;
        BackBufferDesc.Width     = m_SwapChainDesc.Width;
        BackBufferDesc.Height    = m_SwapChainDesc.Height;
        BackBufferDesc.Format    = m_SwapChainDesc.ColorBufferFormat;
        BackBufferDesc.BindFlags = SwapChainUsageFlagsToBindFlags(m_SwapChainDesc.Usage);
        BackBufferDesc.MipLevels = 1;

        RefCntAutoPtr<TextureVkImpl> pBackBufferTex;
        m_pRenderDevice.RawPtr<RenderDeviceVkImpl>()->CreateTexture(BackBufferDesc, swapchainImages[i], RESOURCE_STATE_UNDEFINED, &pBackBufferTex);

        TextureViewDesc RTVDesc;
        RTVDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        RefCntAutoPtr<ITextureView> pRTV;
        pBackBufferTex->CreateView(RTVDesc, &pRTV);
        m_pBackBufferRTV[i] = RefCntAutoPtr<ITextureViewVk>(pRTV, IID_TextureViewVk);
    }

    if (m_SwapChainDesc.DepthBufferFormat != TEX_FORMAT_UNKNOWN)
    {
        TextureDesc DepthBufferDesc;
        DepthBufferDesc.Type        = RESOURCE_DIM_TEX_2D;
        DepthBufferDesc.Width       = m_SwapChainDesc.Width;
        DepthBufferDesc.Height      = m_SwapChainDesc.Height;
        DepthBufferDesc.Format      = m_SwapChainDesc.DepthBufferFormat;
        DepthBufferDesc.SampleCount = 1;
        DepthBufferDesc.Usage       = USAGE_DEFAULT;
        DepthBufferDesc.BindFlags   = BIND_DEPTH_STENCIL;

        DepthBufferDesc.ClearValue.Format               = DepthBufferDesc.Format;
        DepthBufferDesc.ClearValue.DepthStencil.Depth   = m_SwapChainDesc.DefaultDepthValue;
        DepthBufferDesc.ClearValue.DepthStencil.Stencil = m_SwapChainDesc.DefaultStencilValue;
        DepthBufferDesc.Name                            = "Main depth buffer";
        RefCntAutoPtr<ITexture> pDepthBufferTex;
        m_pRenderDevice->CreateTexture(DepthBufferDesc, nullptr, static_cast<ITexture**>(&pDepthBufferTex));
        ITextureView* pDSV = pDepthBufferTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        m_pDepthBufferDSV  = RefCntAutoPtr<ITextureViewVk>(pDSV, IID_TextureViewVk);
    }
}

void SwapChainVkImpl::ThrottleFrameSubmission()
{
    if (m_FrameIndex > m_SwapChainDesc.BufferCount)
    {
        m_FrameCompleteFence->Wait(m_FrameIndex - m_SwapChainDesc.BufferCount);
    }
}

VkResult SwapChainVkImpl::AcquireNextImage(DeviceContextVkImpl* pDeviceCtxVk)
{
    RenderDeviceVkImpl*                   pDeviceVk     = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
    const VulkanUtilities::LogicalDevice& LogicalDevice = pDeviceVk->GetLogicalDevice();

    // Applications should not rely on vkAcquireNextImageKHR blocking in order to
    // meter their rendering speed. The implementation may return from this function
    // immediately regardless of how many presentation requests are queued, and regardless
    // of when queued presentation requests will complete relative to the call. Instead,
    // applications can use fence to meter their frame generation work to match the
    // presentation rate.

    // vkAcquireNextImageKHR requires that the semaphore is not in use, so we must wait
    // for the frame (FrameIndex - BufferCount) to complete.
    // This also ensures that there are no more than BufferCount frames in flight at any time.
    ThrottleFrameSubmission();

    RefCntAutoPtr<ManagedSemaphore>& ImageAcquiredSemaphore = m_ImageAcquiredSemaphores[m_SemaphoreIndex];

    VkResult res    = vkAcquireNextImageKHR(LogicalDevice.GetVkDevice(), m_VkSwapChain, UINT64_MAX, ImageAcquiredSemaphore->Get(), VK_NULL_HANDLE, &m_BackBufferIndex);
    m_ImageAcquired = (res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR);
#if PLATFORM_APPLE
    if (res == VK_SUBOPTIMAL_KHR)
    {
        // https://github.com/KhronosGroup/MoltenVK/issues/2542
        m_ImageAcquired = false;
    }
#endif
    if (m_ImageAcquired)
    {
        // Next command in the device context must wait for the next image to be acquired.
        // Unlike fences or events, the act of waiting for a semaphore also unsignals that semaphore.
        // Swapchain image may be used as render target or as destination for copy command.
        pDeviceCtxVk->AddWaitSemaphore(ImageAcquiredSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT);
        if (!m_SwapChainImagesInitialized[m_BackBufferIndex])
        {
            // Vulkan validation layers do not like uninitialized memory.
            // Clear back buffer first time we acquire it.

            ITextureView* pRTV = GetCurrentBackBufferRTV();
            ITextureView* pDSV = GetDepthBufferDSV();
            pDeviceCtxVk->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            pDeviceCtxVk->ClearRenderTarget(GetCurrentBackBufferRTV(), nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            m_SwapChainImagesInitialized[m_BackBufferIndex] = true;
        }
        pDeviceCtxVk->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    return res;
}

void SwapChainVkImpl::Present(Uint32 SyncInterval)
{
    if (SyncInterval != 0 && SyncInterval != 1)
        LOG_WARNING_MESSAGE_ONCE("Vulkan only supports 0 and 1 present intervals");

    RefCntAutoPtr<IDeviceContext> pDeviceContext = m_wpDeviceContext.Lock();
    if (!pDeviceContext)
    {
        LOG_ERROR_MESSAGE("Immediate context has been released");
        return;
    }

    DeviceContextVkImpl* pImmediateCtxVk = pDeviceContext.RawPtr<DeviceContextVkImpl>();
    RenderDeviceVkImpl*  pDeviceVk       = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();

    ITexture* pBackBuffer = GetCurrentBackBufferRTV()->GetTexture();
    pImmediateCtxVk->UnbindTextureFromFramebuffer(ClassPtrCast<TextureVkImpl>(pBackBuffer), false);

    // To properly handle the case where vkAcquireNextImageKHR returns the same index twice in a row, use
    // a separate semaphore per swapchain image and index these semaphores using the index of the acquired image
    // https://github.com/DiligentGraphics/DiligentCore/issues/682
    RefCntAutoPtr<ManagedSemaphore>& DrawCompleteSemaphore = m_DrawCompleteSemaphores[m_BackBufferIndex];
    if (m_ImageAcquired && !m_IsMinimized)
    {
        // TransitionImageLayout() never triggers flush
        pImmediateCtxVk->TransitionImageLayout(pBackBuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        // The context can be empty if no render commands were issued by the app
        //VERIFY(pImmediateCtxVk->GetNumCommandsInCtx() != 0, "The context must not be flushed");
        pImmediateCtxVk->AddSignalSemaphore(DrawCompleteSemaphore);
    }

    pImmediateCtxVk->EnqueueSignal(m_FrameCompleteFence, m_FrameIndex++);
    pImmediateCtxVk->Flush();

    if (!m_IsMinimized)
    {
        VkResult Result = VK_ERROR_OUT_OF_DATE_KHR;
        // Only present if the image was acquired successfully
        if (m_ImageAcquired)
        {
            VkPresentInfoKHR PresentInfo{};
            PresentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            PresentInfo.pNext              = nullptr;
            PresentInfo.waitSemaphoreCount = 1;
            // Unlike fences or events, the act of waiting for a semaphore also unsignals that semaphore
            VkSemaphore WaitSemaphore[] = {DrawCompleteSemaphore->Get()};
            PresentInfo.pWaitSemaphores = WaitSemaphore;
            PresentInfo.swapchainCount  = 1;
            PresentInfo.pSwapchains     = &m_VkSwapChain;
            PresentInfo.pImageIndices   = &m_BackBufferIndex;
            PresentInfo.pResults        = &Result;
            pDeviceVk->LockCmdQueueAndRun(
                pImmediateCtxVk->GetCommandQueueId(),
                [&PresentInfo](ICommandQueueVk* pCmdQueueVk) //
                {
                    pCmdQueueVk->Present(PresentInfo);
                } //
            );
        }

        if (Result == VK_SUBOPTIMAL_KHR || Result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateVulkanSwapchain(pImmediateCtxVk);
            m_SemaphoreIndex = m_SwapChainDesc.BufferCount - 1; // To start with 0 index when acquire next image
        }
        else
        {
            DEV_CHECK_ERR(Result == VK_SUCCESS, "Present failed");
        }
    }

    if (m_SwapChainDesc.IsPrimary)
    {
        pImmediateCtxVk->FinishFrame();
        pDeviceVk->ReleaseStaleResources();
    }

    if (!m_IsMinimized)
    {
        ++m_SemaphoreIndex;
        if (m_SemaphoreIndex >= m_SwapChainDesc.BufferCount)
            m_SemaphoreIndex = 0;

        bool EnableVSync = SyncInterval != 0;

        VkResult res = (m_VSyncEnabled == EnableVSync) ? AcquireNextImage(pImmediateCtxVk) : VK_ERROR_OUT_OF_DATE_KHR;
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_VSyncEnabled = EnableVSync;
            RecreateVulkanSwapchain(pImmediateCtxVk);
            m_SemaphoreIndex = m_SwapChainDesc.BufferCount - 1; // To start with 0 index when acquire next image

            res = AcquireNextImage(pImmediateCtxVk);

#if PLATFORM_APPLE
            // For some reason, on MoltenVk we may get VK_SUBOPTIMAL_KHR first time we
            // acquire the image after the swap chain has been recreated.
            // Recreating it yet again seems to fix the problem.
            if (res == VK_SUBOPTIMAL_KHR)
            {
                RecreateVulkanSwapchain(pImmediateCtxVk);
                res = AcquireNextImage(pImmediateCtxVk);
            }
#endif
        }
        // The image may still be out of date if the window keeps changing size
    }
    else
    {
        // Throttle frame submission to make sure that resources are released
        // https://github.com/DiligentGraphics/DiligentSamples/issues/234
        ThrottleFrameSubmission();
    }
}

void SwapChainVkImpl::ReleaseSwapChainResources(DeviceContextVkImpl* pImmediateCtxVk, bool DestroyVkSwapChain)
{
    if (m_VkSwapChain == VK_NULL_HANDLE)
        return;

    if (pImmediateCtxVk != nullptr)
    {
        // Flush to submit all pending commands and semaphores to the queue.
        pImmediateCtxVk->Flush();

        bool RenderTargetsReset = false;
        for (Uint32 i = 0; i < m_pBackBufferRTV.size() && !RenderTargetsReset; ++i)
        {
            TextureVkImpl* pCurrentBackBuffer = ClassPtrCast<TextureVkImpl>(m_pBackBufferRTV[i]->GetTexture());
            RenderTargetsReset                = pImmediateCtxVk->UnbindTextureFromFramebuffer(pCurrentBackBuffer, false);
        }
        if (RenderTargetsReset)
        {
            LOG_INFO_MESSAGE_ONCE("The swap chain's back and depth-stencil buffers were unbound from the device context because "
                                  "the swap chain is being destroyed. An application should use SetRenderTargets() to restore them.");
        }
    }

    RenderDeviceVkImpl* pDeviceVk = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();

    // This will release references to Vk swap chain buffers hold by
    // m_pBackBufferRTV[].
    pDeviceVk->IdleGPU();

    // Just idling the GPU is not enough and results in validation warnings.
    // As a matter of fact, it is only required to check the fence status.
    if (m_FrameIndex > 1)
    {
        m_FrameCompleteFence->Wait(m_FrameIndex - 1);
    }

    // All references to the swap chain must be released before it can be destroyed
    m_pBackBufferRTV.clear();
    m_SwapChainImagesInitialized.clear();
    m_pDepthBufferDSV.Release();

    // We must wait until GPU is idled before destroying the fences as they
    // are destroyed immediately. The semaphores are managed and will be kept alive
    // by the device context they are submitted to.
    m_ImageAcquiredSemaphores.clear();
    m_DrawCompleteSemaphores.clear();
    m_SemaphoreIndex = 0;

    if (DestroyVkSwapChain)
    {
        vkDestroySwapchainKHR(pDeviceVk->GetVkDevice(), m_VkSwapChain, NULL);
        m_VkSwapChain = VK_NULL_HANDLE;
    }
}

void SwapChainVkImpl::RecreateVulkanSwapchain(DeviceContextVkImpl* pImmediateCtxVk)
{
    // Do not destroy Vulkan swap chain as we will use it as oldSwapchain parameter.
    ReleaseSwapChainResources(pImmediateCtxVk, /*DestroyVkSwapChain*/ false);

    // Check if the surface is lost
    {
        RenderDeviceVkImpl*    pDeviceVk      = m_pRenderDevice.RawPtr<RenderDeviceVkImpl>();
        const VkPhysicalDevice vkDeviceHandle = pDeviceVk->GetPhysicalDevice().GetVkDeviceHandle();

        VkSurfaceCapabilitiesKHR surfCapabilities;
        // Call vkGetPhysicalDeviceSurfaceCapabilitiesKHR only to check the return code
        VkResult err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDeviceHandle, m_VkSurface, &surfCapabilities);
        if (err == VK_ERROR_SURFACE_LOST_KHR)
        {
            // Destroy the swap chain associated with the surface
            if (m_VkSwapChain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(pDeviceVk->GetVkDevice(), m_VkSwapChain, NULL);
                m_VkSwapChain = VK_NULL_HANDLE;
            }

            // Recreate the surface
            CreateSurface();
        }
    }

    CreateVulkanSwapChain();
    InitBuffersAndViews();
}

void SwapChainVkImpl::Resize(Uint32 NewWidth, Uint32 NewHeight, SURFACE_TRANSFORM NewPreTransform)
{
    bool RecreateSwapChain = false;

#if PLATFORM_ANDROID
    if (m_VkSurface != VK_NULL_HANDLE)
    {
        // Check orientation
        const RenderDeviceVkImpl*              pRenderDeviceVk = m_pRenderDevice.ConstPtr<RenderDeviceVkImpl>();
        const VulkanUtilities::PhysicalDevice& PhysicalDevice  = pRenderDeviceVk->GetPhysicalDevice();
        const VkPhysicalDevice                 vkDeviceHandle  = PhysicalDevice.GetVkDeviceHandle();

        VkSurfaceCapabilitiesKHR surfCapabilities = {};

        VkResult err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkDeviceHandle, m_VkSurface, &surfCapabilities);
        if (err == VK_SUCCESS)
        {
            if (m_CurrentSurfaceTransform != surfCapabilities.currentTransform)
            {
                // Surface orientation has changed - we need to recreate the swap chain
                RecreateSwapChain = true;
            }

            constexpr auto Rotate90TransformFlags =
                VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR |
                VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR |
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR |
                VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR;

            if (NewWidth == 0 || NewHeight == 0)
            {
                NewWidth  = m_SurfaceIdentityExtent.width;
                NewHeight = m_SurfaceIdentityExtent.height;

                if ((surfCapabilities.currentTransform & Rotate90TransformFlags) != 0)
                {
                    // Swap to get logical dimensions as input NewWidth and NewHeight are
                    // expected to be logical sizes.
                    std::swap(NewWidth, NewHeight);
                }
            }

            if (NewPreTransform == SURFACE_TRANSFORM_OPTIMAL)
            {
                if ((surfCapabilities.currentTransform & Rotate90TransformFlags) != 0)
                {
                    // Swap to get physical dimensions
                    std::swap(NewWidth, NewHeight);
                }
            }
            else
            {
                // Swap if necessary to get desired sizes after pre-transform
                if (NewPreTransform == SURFACE_TRANSFORM_ROTATE_90 ||
                    NewPreTransform == SURFACE_TRANSFORM_ROTATE_270 ||
                    NewPreTransform == SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90 ||
                    NewPreTransform == SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270)
                {
                    std::swap(NewWidth, NewHeight);
                }
            }
        }
        else
        {
            LOG_ERROR_MESSAGE(err, "Failed to query physical device surface capabilities");
        }
    }
#endif

    if (TSwapChainBase::Resize(NewWidth, NewHeight, NewPreTransform))
        RecreateSwapChain = true;

    if (RecreateSwapChain)
    {
        RefCntAutoPtr<IDeviceContext> pDeviceContext = m_wpDeviceContext.Lock();
        VERIFY(pDeviceContext, "Immediate context has been released");
        if (pDeviceContext)
        {
            try
            {
                DeviceContextVkImpl* pImmediateCtxVk = pDeviceContext.RawPtr<DeviceContextVkImpl>();
                // RecreateVulkanSwapchain() unbinds default FB
                RecreateVulkanSwapchain(pImmediateCtxVk);

                AcquireNextImage(pImmediateCtxVk);
                // The image may be immediately out of date if the window keeps being resized
            }
            catch (const std::runtime_error&)
            {
                LOG_ERROR("Failed to resize the swap chain");
            }
        }
    }

    m_IsMinimized = (NewWidth == 0 && NewHeight == 0);
}


void SwapChainVkImpl::SetFullscreenMode(const DisplayModeAttribs& DisplayMode)
{
}

void SwapChainVkImpl::SetWindowedMode()
{
}

} // namespace Diligent
