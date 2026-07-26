#pragma once
#include "Context.h"
#include "Swapchain.h"
#include "FrameSync.h"
#include "Allocator.h"

struct FrameInfo {
    uint32_t imageIndex;
    VkImageView imageView;
    VkCommandBuffer commandBuffer;
    uint32_t frameIndex;
    VkExtent2D extent;
};

class App {
public:
    virtual ~App() = default;

    virtual void init() = 0;
    virtual void recordFrame(const FrameInfo& frame) = 0;
    virtual void cleanup() = 0;

    virtual void resize(uint32_t w, uint32_t h) {}
    virtual void update(float dt) {}

    void run(const char* title, uint32_t w, uint32_t h);

    VulkanContext& getContext() { return ctx; }
    VulkanSwapchain& getSwapchain() { return swap; }
    VulkanFrameSync& getFrameSync() { return sync; }
    VulkanAllocator& getAllocator() { return alloc; }

private:
    void recreateSwapchain();
    bool acquireNextFrame(uint32_t& imageIndex);
    void submitFrame(uint32_t imageIndex);

    VulkanContext ctx;
    VulkanSwapchain swap;
    VulkanFrameSync sync;
    VulkanAllocator alloc;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> imagesInFlight;

    bool running = false;
    float lastTime = 0.0f;
};
