#include "Context.h"
#include <set>
#include <cstring>
#include <algorithm>

bool VulkanContext::init(SDL_Window* window, bool validation) {
    this->window = window;
    this->enableValidation = validation;
    if (volkInitialize() != VK_SUCCESS) return false;
    if (!createInstance()) return false;
    if (!createSurface()) return false;
    if (!pickPhysicalDevice()) return false;
    queryFeatureSupport();
    if (!createLogicalDevice()) return false;
    return true;
}

void VulkanContext::shutdown() {
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}

bool VulkanContext::createInstance() {
    if (enableValidation) {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const auto& layer : availableLayers) {
            if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                activeLayers.push_back("VK_LAYER_KHRONOS_validation");
                break;
            }
        }

        if (!activeLayers.empty()) {
            activeExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    for (uint32_t i = 0; i < sdlExtensionCount; i++) {
        activeExtensions.push_back(sdlExtensions[i]);
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "SDL_VulkanCodotaku";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "vkbootstrap";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(activeExtensions.size());
    createInfo.ppEnabledExtensionNames = activeExtensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(activeLayers.size());
    createInfo.ppEnabledLayerNames = activeLayers.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }

    volkLoadInstance(instance);
    return true;
}

bool VulkanContext::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        return false;
    }
    return true;
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& dev : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        bool graphicsFound = false;
        bool presentFound = false;

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueueFamily = i;
                graphicsFound = true;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (presentSupport) {
                presentQueueFamily = i;
                presentFound = true;
            }

            if (graphicsFound && presentFound) break;
        }

        if (graphicsFound && presentFound) {
            physicalDevice = dev;
            vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &vulkan12Features;

            vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            vulkan12Features.pNext = &vulkan13Features;

            vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            vulkan13Features.pNext = &vulkan14Features;

            vulkan14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

            vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

            return true;
        }
    }
    return false;
}

void VulkanContext::queryFeatureSupport() {
    VkPhysicalDeviceVulkan12Features supported12 = vulkan12Features;
    VkPhysicalDeviceVulkan13Features supported13 = vulkan13Features;

    vulkan12Features = {};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.bufferDeviceAddress = supported12.bufferDeviceAddress ? VK_TRUE : VK_FALSE;
    vulkan12Features.descriptorIndexing = supported12.descriptorIndexing ? VK_TRUE : VK_FALSE;
    vulkan12Features.drawIndirectCount = supported12.drawIndirectCount ? VK_TRUE : VK_FALSE;

    vulkan13Features = {};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.dynamicRendering = supported13.dynamicRendering ? VK_TRUE : VK_FALSE;
    vulkan13Features.synchronization2 = supported13.synchronization2 ? VK_TRUE : VK_FALSE;

    deviceFeatures2.pNext = &vulkan12Features;
    vulkan12Features.pNext = &vulkan13Features;
    vulkan13Features.pNext = &vulkan14Features;
}

bool VulkanContext::createLogicalDevice() {
    std::set<uint32_t> uniqueQueueFamilies = {graphicsQueueFamily, presentQueueFamily};

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pNext = &deviceFeatures2;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        return false;
    }

    volkLoadDevice(device);

    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);

    return true;
}
