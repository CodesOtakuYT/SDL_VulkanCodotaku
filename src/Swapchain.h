#pragma once
#include <volk.h>
#include <glm/glm.hpp>
#include <vector>

struct VulkanSwapchain {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    glm::uvec2 extent{0, 0};
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    void init(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
              uint32_t graphicsQueueFamily, uint32_t presentQueueFamily);
    void shutdown(VkDevice device);

    void recreate(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                  uint32_t graphicsQueueFamily, uint32_t presentQueueFamily);

    VkResult acquireNextImage(VkDevice device, VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex);
    VkResult present(VkQueue presentQueue, VkSemaphore renderFinishedSemaphore, uint32_t imageIndex);

private:
    void chooseSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void choosePresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void chooseCompositeAlpha(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
    void chooseExtent(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, glm::uvec2 minExtent);
    void createSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                         uint32_t graphicsQueueFamily, uint32_t presentQueueFamily);
    void getSwapchainImages(VkDevice device);
    void createImageViews(VkDevice device);
};
