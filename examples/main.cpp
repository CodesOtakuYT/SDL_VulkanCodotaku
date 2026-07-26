#include "Context.h"
#include "Swapchain.h"
#include "FrameSync.h"
#include "Allocator.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* msg) {
    if (!ok) {
        fprintf(stderr, "FATAL: %s\n", msg);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    check(SDL_Init(SDL_INIT_VIDEO), "SDL_Init failed");

    SDL_Window* window = SDL_CreateWindow(
        "SDL_VulkanCodotaku",
        1280, 720,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    check(window, "SDL_CreateWindow failed");

    VulkanContext ctx;
    check(ctx.init(window), "VulkanContext::init failed");
    printf("Device: %s\n", ctx.deviceProperties.deviceName);

    VulkanSwapchain swap;
    check(swap.init(ctx.physicalDevice, ctx.device, ctx.surface,
                     ctx.graphicsQueueFamily, ctx.presentQueueFamily),
          "VulkanSwapchain::init failed");
    printf("Swapchain: %ux%u, %zu images\n", swap.extent.width, swap.extent.height, swap.images.size());

    VulkanFrameSync sync;
    check(sync.init(ctx.device, static_cast<uint32_t>(swap.images.size())),
          "VulkanFrameSync::init failed");

    VulkanAllocator alloc;
    check(alloc.init(ctx.instance, ctx.physicalDevice, ctx.device),
          "VulkanAllocator::init failed");

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily;
    VkCommandPool commandPool;
    check(vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS,
          "CreateCommandPool failed");

    std::vector<VkCommandBuffer> commandBuffers(VulkanFrameSync::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    check(vkAllocateCommandBuffers(ctx.device, &allocInfo, commandBuffers.data()) == VK_SUCCESS,
          "AllocateCommandBuffers failed");

    std::vector<VkFence> imagesInFlight(swap.images.size(), VK_NULL_HANDLE);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                vkDeviceWaitIdle(ctx.device);
                sync.shutdown(ctx.device);
                swap.recreate(ctx.physicalDevice, ctx.device, ctx.surface,
                              ctx.graphicsQueueFamily, ctx.presentQueueFamily);
                sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
                imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);
    }
        }

        sync.waitForFence(ctx.device);
        sync.resetFence(ctx.device);

        uint32_t imageIndex;
        VkResult acquireResult = swap.acquireNextImage(ctx.device, sync.getImageAvailableSemaphore(), &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
            sync.shutdown(ctx.device);
            swap.recreate(ctx.physicalDevice, ctx.device, ctx.surface,
                          ctx.graphicsQueueFamily, ctx.presentQueueFamily);
            sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
            imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);
            continue;
        }

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(ctx.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = sync.getInFlightFence();

        uint32_t frame = sync.currentFrame;
        vkResetCommandBuffer(commandBuffers[frame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffers[frame], &beginInfo);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = swap.images[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffers[frame],
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkClearValue clearColor = {{{0.0f, 0.8f, 0.4f, 1.0f}}};
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = swap.imageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearColor;

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.renderArea.offset = {0, 0};
        renderInfo.renderArea.extent = swap.extent;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(commandBuffers[frame], &renderInfo);
        vkCmdEndRendering(commandBuffers[frame]);

        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(commandBuffers[frame],
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkEndCommandBuffer(commandBuffers[frame]);

        VkSemaphore waitSemaphores[] = {sync.getImageAvailableSemaphore()};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {sync.getRenderFinishedSemaphore(imageIndex)};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[frame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkResult submitResult = vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, sync.getInFlightFence());
        if (submitResult != VK_SUCCESS) {
            fprintf(stderr, "vkQueueSubmit failed: %d\n", submitResult);
            running = false;
        }

        sync.advanceFrame();

        VkResult presentResult = swap.present(ctx.presentQueue, sync.getRenderFinishedSemaphore(imageIndex), imageIndex);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            sync.shutdown(ctx.device);
            swap.recreate(ctx.physicalDevice, ctx.device, ctx.surface,
                          ctx.graphicsQueueFamily, ctx.presentQueueFamily);
            sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
            imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);
        }

}

    vkDeviceWaitIdle(ctx.device);
    vkDestroyCommandPool(ctx.device, commandPool, nullptr);
    alloc.shutdown();
    sync.shutdown(ctx.device);
    swap.shutdown(ctx.device);
    ctx.shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
