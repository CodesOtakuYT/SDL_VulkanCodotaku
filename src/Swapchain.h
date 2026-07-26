#pragma once
#include <volk.h>
#include <vector>
#include <functional>

struct VulkanSwapchain {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    bool init(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
              uint32_t graphicsQueueFamily, uint32_t presentQueueFamily);
    void shutdown(VkDevice device);

    void recreate(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                  uint32_t graphicsQueueFamily, uint32_t presentQueueFamily);

    VkResult acquireNextImage(VkDevice device, VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex);
    VkResult present(VkQueue presentQueue, VkSemaphore renderFinishedSemaphore, uint32_t imageIndex);

private:
    bool chooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    bool choosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    bool chooseCompositeAlpha(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    bool chooseExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    bool createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                         uint32_t graphicsQueueFamily, uint32_t presentQueueFamily);
    bool getSwapchainImages(VkDevice device);
    bool createImageViews(VkDevice device);
};
