#include "FrameSync.h"
#include "VkError.h"

void VulkanFrameSync::init(VkDevice device, uint32_t swapchainImageCount) {
    imageCount = swapchainImageCount;
    createSemaphores(device);
    createFences(device);
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

void VulkanFrameSync::createSemaphores(VkDevice device) {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(imageCount);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCheck(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
                "vkCreateSemaphore (imageAvailable) failed");
    }
    for (uint32_t i = 0; i < imageCount; i++) {
        vkCheck(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]),
                "vkCreateSemaphore (renderFinished) failed");
    }
}

void VulkanFrameSync::createFences(VkDevice device) {
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCheck(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]),
                "vkCreateFence failed");
    }
}
