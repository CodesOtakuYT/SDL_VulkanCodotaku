#pragma once
#include <volk.h>
#include <vector>
#include <cstdint>
#include <algorithm>

struct VulkanFrameSync {
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    uint32_t imageCount = 0;

    bool init(VkDevice device, uint32_t swapchainImageCount);
    void shutdown(VkDevice device);

    void waitForFence(VkDevice device);
    void resetFence(VkDevice device);
    void advanceFrame();

    VkSemaphore getImageAvailableSemaphore() const { return imageAvailableSemaphores[currentFrame]; }
    VkSemaphore getRenderFinishedSemaphore(uint32_t swapchainImageIndex) const { return renderFinishedSemaphores[swapchainImageIndex]; }
    VkFence getInFlightFence() const { return inFlightFences[currentFrame]; }

private:
    bool createSemaphores(VkDevice device);
    bool createFences(VkDevice device);
};
