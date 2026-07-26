#include "App.h"
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

void App::run(const char* title, uint32_t w, uint32_t h) {
    check(SDL_Init(SDL_INIT_VIDEO), "SDL_Init failed");

    SDL_Window* window = SDL_CreateWindow(title, w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    check(window, "SDL_CreateWindow failed");

    check(ctx.init(window), "VulkanContext::init failed");
    printf("Device: %s\n", ctx.deviceProperties.deviceName);

    check(swap.init(ctx.physicalDevice, ctx.device, ctx.surface,
                     ctx.graphicsQueueFamily, ctx.presentQueueFamily),
          "VulkanSwapchain::init failed");
    printf("Swapchain: %ux%u, %zu images\n", swap.extent.width, swap.extent.height, swap.images.size());

    check(sync.init(ctx.device, static_cast<uint32_t>(swap.images.size())),
          "VulkanFrameSync::init failed");

    check(alloc.init(ctx.instance, ctx.physicalDevice, ctx.device),
          "VulkanAllocator::init failed");

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily;
    check(vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS,
          "CreateCommandPool failed");

    commandBuffers.resize(VulkanFrameSync::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    check(vkAllocateCommandBuffers(ctx.device, &allocInfo, commandBuffers.data()) == VK_SUCCESS,
          "AllocateCommandBuffers failed");

    imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);

    init();

    running = true;
    lastTime = static_cast<float>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();

    while (running) {
        float now = static_cast<float>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
        float dt = now - lastTime;
        lastTime = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                recreateSwapchain();
                resize(swap.extent.width, swap.extent.height);
            }
        }

        update(dt);

        uint32_t imageIndex;
        if (!acquireNextFrame(imageIndex)) continue;

        uint32_t frame = sync.currentFrame;
        vkResetCommandBuffer(commandBuffers[frame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffers[frame], &beginInfo);

        recordFrame(FrameInfo{imageIndex, swap.imageViews[imageIndex], commandBuffers[frame], frame, swap.extent});

        vkEndCommandBuffer(commandBuffers[frame]);

        submitFrame(imageIndex);
        sync.advanceFrame();
    }

    vkDeviceWaitIdle(ctx.device);

    cleanup();

    vkDestroyCommandPool(ctx.device, commandPool, nullptr);
    alloc.shutdown();
    sync.shutdown(ctx.device);
    swap.shutdown(ctx.device);
    ctx.shutdown();

    SDL_Quit();
}

void App::recreateSwapchain() {
    vkDeviceWaitIdle(ctx.device);
    sync.shutdown(ctx.device);
    swap.recreate(ctx.physicalDevice, ctx.device, ctx.surface,
                  ctx.graphicsQueueFamily, ctx.presentQueueFamily);
    sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
    imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);
}

bool App::acquireNextFrame(uint32_t& imageIndex) {
    sync.waitForFence(ctx.device);
    sync.resetFence(ctx.device);

    VkResult result = swap.acquireNextImage(ctx.device, sync.getImageAvailableSemaphore(), &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
        resize(swap.extent.width, swap.extent.height);
        return false;
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(ctx.device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[imageIndex] = sync.getInFlightFence();

    return true;
}

void App::submitFrame(uint32_t imageIndex) {
    uint32_t frame = sync.currentFrame;

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

    VkResult result = vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, sync.getInFlightFence());
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkQueueSubmit failed: %d\n", result);
        running = false;
        return;
    }

    result = swap.present(ctx.presentQueue, sync.getRenderFinishedSemaphore(imageIndex), imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
        resize(swap.extent.width, swap.extent.height);
    }
}
