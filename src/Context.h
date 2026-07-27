#pragma once
#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct VulkanContext {
    SDL_Window* window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    uint32_t presentQueueFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties deviceProperties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    VkPhysicalDeviceVulkan14Features vulkan14Features{};
    VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR unifiedImageLayoutsFeatures{};

    bool enableValidation = false;
    std::vector<const char*> activeLayers;
    std::vector<const char*> activeExtensions;

    void init(SDL_Window* window, bool validation = true);
    void shutdown();

private:
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void queryFeatureSupport();
};
