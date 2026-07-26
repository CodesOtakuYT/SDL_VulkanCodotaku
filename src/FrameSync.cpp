#include "FrameSync.h"

bool VulkanFrameSync::init(VkDevice device, uint32_t swapchainImageCount) {
    imageCount = swapchainImageCount;
    if (!createSemaphores(device)) return false;
    if (!createFences(device)) return false;
    return true;
}

void VulkanFrameSync::shutdown(VkDevice device) {
    for (auto fence : inFlightFences) {
        vkDestroyFence(device, fence, nullptr);
    }
    inFlightFences.clear();

    for (auto semaphore : renderFinishedSemaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    renderFinishedSemaphores.clear();

    for (auto semaphore : imageAvailableSemaphores) {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
    imageAvailableSemaphores.clear();
}

void VulkanFrameSync::waitForFence(VkDevice device) {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
}

void VulkanFrameSync::resetFence(VkDevice device) {
    vkResetFences(device, 1, &inFlightFences[currentFrame]);
}

void VulkanFrameSync::advanceFrame() {
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanFrameSync::createSemaphores(VkDevice device) {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(imageCount);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS) {
            return false;
        }
    }
    for (uint32_t i = 0; i < imageCount; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanFrameSync::createFences(VkDevice device) {
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}
