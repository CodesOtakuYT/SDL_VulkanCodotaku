#include "App.h"
#include "VkError.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cstdio>

ShaderModule App::compileShader(const std::string& source,
                                VkShaderStageFlagBits stage,
                                const std::string& filename) {
    ShaderModule mod;
    mod.createFromGLSL(ctx.device, compiler, source, stage, filename);
    return mod;
}

void App::run(const char* title, uint32_t w, uint32_t h) {
    try {
        if (!SDL_Init(SDL_INIT_VIDEO)) throw VkbError("SDL_Init failed");

        window = SDL_CreateWindow(title, w, h, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window) throw VkbError("SDL_CreateWindow failed");

        ctx.init(window);
        printf("Device: %s\n", ctx.deviceProperties.deviceName);

        swap.init(ctx.physicalDevice, ctx.device, ctx.surface,
                  ctx.graphicsQueueFamily, ctx.presentQueueFamily);
        printf("Swapchain: %ux%u, %zu images\n", swap.extent.width, swap.extent.height, swap.images.size());

        sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
        alloc.init(ctx.instance, ctx.physicalDevice, ctx.device);
        compiler.init();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = ctx.graphicsQueueFamily;
        vkCheck(vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &commandPool), "vkCreateCommandPool failed");

        commandBuffers.resize(VulkanFrameSync::MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        vkCheck(vkAllocateCommandBuffers(ctx.device, &allocInfo, commandBuffers.data()),
                "vkAllocateCommandBuffers failed");

        imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);

        SDL_AddEventWatch(+[](void* userdata, SDL_Event* event) -> bool {
            auto* app = static_cast<App*>(userdata);
            if (event->type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
                app->state = AppState::Backgrounded;
                app->background();
            } else if (event->type == SDL_EVENT_DID_ENTER_FOREGROUND) {
                app->state = AppState::Running;
                app->recreateSurfaceAndSwapchain();
                app->foreground();
            }
            return true;
        }, this);

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
                if (event.type == SDL_EVENT_WINDOW_RESIZED && state == AppState::Running) {
                    int w = 0, h = 0;
                    SDL_GetWindowSize(window, &w, &h);
                    if (w > 0 && h > 0) {
                        recreateSwapchain();
                        resize(swap.extent.width, swap.extent.height);
                    }
                }
            }

            if (state == AppState::Backgrounded) continue;

            int w = 0, h = 0;
            SDL_GetWindowSize(window, &w, &h);
            if (w <= 0 || h <= 0) continue;

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
        compiler.shutdown();
        alloc.shutdown();
        sync.shutdown(ctx.device);
        swap.shutdown(ctx.device);
        ctx.shutdown();

        SDL_DestroyWindow(window);
        SDL_Quit();
    } catch (const VkbError& e) {
        fprintf(stderr, "FATAL: %s\n", e.what());
        throw;
    }
}

void App::recreateSwapchain() {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) return;

    vkDeviceWaitIdle(ctx.device);
    sync.shutdown(ctx.device);
    swap.recreate(ctx.physicalDevice, ctx.device, ctx.surface,
                  ctx.graphicsQueueFamily, ctx.presentQueueFamily);
    sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
    imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);
}

void App::recreateSurfaceAndSwapchain() {
    vkDeviceWaitIdle(ctx.device);

    if (ctx.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        ctx.surface = VK_NULL_HANDLE;
    }

    if (!SDL_Vulkan_CreateSurface(window, ctx.instance, nullptr, &ctx.surface)) {
        throw VkbError("Surface recreation failed");
    }

    sync.shutdown(ctx.device);
    swap.shutdown(ctx.device);

    swap.init(ctx.physicalDevice, ctx.device, ctx.surface,
              ctx.graphicsQueueFamily, ctx.presentQueueFamily);
    sync.init(ctx.device, static_cast<uint32_t>(swap.images.size()));
    imagesInFlight.assign(swap.images.size(), VK_NULL_HANDLE);
}

bool App::acquireNextFrame(uint32_t& imageIndex) {
    sync.waitForFence(ctx.device);
    sync.resetFence(ctx.device);

    VkResult result = swap.acquireNextImage(ctx.device, sync.getImageAvailableSemaphore(), &imageIndex);

    switch (result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;

    case VK_ERROR_OUT_OF_DATE_KHR:
        recreateSwapchain();
        resize(swap.extent.width, swap.extent.height);
        return false;

    case VK_ERROR_SURFACE_LOST_KHR:
        if (state == AppState::Backgrounded) return false;
        recreateSurfaceAndSwapchain();
        return false;

    default:
        vkCheck(result, "vkAcquireNextImageKHR failed");
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

    vkCheck(vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, sync.getInFlightFence()),
            "vkQueueSubmit failed");

    VkResult result = swap.present(ctx.presentQueue, sync.getRenderFinishedSemaphore(imageIndex), imageIndex);

    switch (result) {
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
        break;

    case VK_ERROR_OUT_OF_DATE_KHR:
        recreateSwapchain();
        resize(swap.extent.width, swap.extent.height);
        break;

    case VK_ERROR_SURFACE_LOST_KHR:
        if (state != AppState::Backgrounded) {
            recreateSurfaceAndSwapchain();
        }
        break;

    default:
        break;
    }
}
