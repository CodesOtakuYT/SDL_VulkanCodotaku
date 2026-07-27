#include "Swapchain.h"
#include "VkError.h"
#include <algorithm>

void VulkanSwapchain::init(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                           uint32_t graphicsQueueFamily, uint32_t presentQueueFamily) {
    chooseSurfaceFormat(physicalDevice, surface);
    choosePresentMode(physicalDevice, surface);
    chooseCompositeAlpha(physicalDevice, surface);
    chooseExtent(physicalDevice, surface, {0, 0});
    createSwapchain(physicalDevice, device, surface, graphicsQueueFamily, presentQueueFamily);
    getSwapchainImages(device);
    createImageViews(device);
}

void VulkanSwapchain::shutdown(VkDevice device) {
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers.clear();

    for (auto imageView : imageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    imageViews.clear();

    images.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::recreate(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                               uint32_t graphicsQueueFamily, uint32_t presentQueueFamily) {
    vkDeviceWaitIdle(device);

    VkSwapchainKHR oldSwapchain = swapchain;
    swapchain = VK_NULL_HANDLE;

    shutdown(device);

    chooseSurfaceFormat(physicalDevice, surface);
    choosePresentMode(physicalDevice, surface);
    chooseCompositeAlpha(physicalDevice, surface);
    chooseExtent(physicalDevice, surface, {0, 0});

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = imageFormat;
    createInfo.imageColorSpace = colorSpace;
    createInfo.imageExtent = {extent.x, extent.y};
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        swapchain = oldSwapchain;
        return;
    }

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }

    getSwapchainImages(device);
    createImageViews(device);
}

VkResult VulkanSwapchain::acquireNextImage(VkDevice device, VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex) {
    return vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, imageIndex);
}

VkResult VulkanSwapchain::present(VkQueue presentQueue, VkSemaphore renderFinishedSemaphore, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    return vkQueuePresentKHR(presentQueue, &presentInfo);
}

void VulkanSwapchain::chooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            imageFormat = f.format;
            colorSpace = f.colorSpace;
            return;
        }
    }
    if (!formats.empty()) {
        imageFormat = formats[0].format;
        colorSpace = formats[0].colorSpace;
        return;
    }
    throw VkbError("No supported surface formats");
}

void VulkanSwapchain::choosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            return;
        }
    }
}

void VulkanSwapchain::chooseCompositeAlpha(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    static const VkCompositeAlphaFlagBitsKHR fallbacks[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };

    for (auto fb : fallbacks) {
        if (capabilities.supportedCompositeAlpha & fb) {
            compositeAlpha = fb;
            return;
        }
    }
    compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

void VulkanSwapchain::chooseExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, glm::uvec2 minExtent) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = {capabilities.currentExtent.width, capabilities.currentExtent.height};
    } else {
        extent.x = std::clamp(minExtent.x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.y = std::clamp(minExtent.y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    if (extent.x == 0 || extent.y == 0) {
        extent.x = std::max(extent.x, 1u);
        extent.y = std::max(extent.y, 1u);
    }
}

void VulkanSwapchain::createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                                       uint32_t graphicsQueueFamily, uint32_t presentQueueFamily) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = imageFormat;
    createInfo.imageColorSpace = colorSpace;
    createInfo.imageExtent = {extent.x, extent.y};
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (graphicsQueueFamily != presentQueueFamily) {
        uint32_t indices[] = {graphicsQueueFamily, presentQueueFamily};
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = indices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = compositeAlpha;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    vkCheck(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain), "vkCreateSwapchainKHR failed");
}

void VulkanSwapchain::getSwapchainImages(VkDevice device) {
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
}

void VulkanSwapchain::createImageViews(VkDevice device) {
    imageViews.resize(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        vkCheck(vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]), "vkCreateImageView failed");
    }
}
