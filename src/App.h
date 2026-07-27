#pragma once
#include "Context.h"
#include "Swapchain.h"
#include "FrameSync.h"
#include "Allocator.h"
#include "Shader.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

struct FrameInfo {
    uint32_t imageIndex;
    VkImageView imageView;
    VkCommandBuffer commandBuffer;
    uint32_t frameIndex;
    glm::uvec2 extent;
};

enum class AppState {
    Running,
    Backgrounded,
};

class App {
public:
    virtual ~App() = default;

    virtual void init() = 0;
    virtual void recordFrame(const FrameInfo& frame) = 0;
    virtual void cleanup() = 0;

    virtual void resize(glm::uvec2 size) {}
    virtual void update(float dt) {}
    virtual void background() {}
    virtual void foreground() {}

    void run(const char* title, glm::uvec2 size);

    ShaderModule compileShader(const std::string& source,
                               VkShaderStageFlagBits stage,
                               const std::string& filename);

    VulkanContext& getContext() { return ctx; }
    VulkanSwapchain& getSwapchain() { return swap; }
    VulkanFrameSync& getFrameSync() { return sync; }
    VulkanAllocator& getAllocator() { return alloc; }
    AppState getState() const { return state; }

private:
    void recreateSwapchain();
    void recreateSurfaceAndSwapchain();
    bool acquireNextFrame(uint32_t& imageIndex);
    void submitFrame(uint32_t imageIndex);

    VulkanContext ctx;
    VulkanSwapchain swap;
    VulkanFrameSync sync;
    VulkanAllocator alloc;
    ShaderCompiler compiler;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkFence> imagesInFlight;

    SDL_Window* window = nullptr;
    AppState state = AppState::Running;
    bool running = false;
    float lastTime = 0.0f;
};
