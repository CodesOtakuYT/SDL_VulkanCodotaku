#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>
#include <cstdint>

struct VulkanAllocator {
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    void init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
    void shutdown();

    VkResult createBuffer(const VkBufferCreateInfo& bufferInfo,
                          const VmaAllocationCreateInfo& allocInfo,
                          VkBuffer* buffer,
                          VmaAllocation* allocation,
                          VmaAllocationInfo* allocInfoOut = nullptr);

    void destroyBuffer(VkBuffer buffer, VmaAllocation allocation);

    VkResult mapMemory(VmaAllocation allocation, void** data);
    void unmapMemory(VmaAllocation allocation);

    VkDeviceSize getBufferDeviceAddress(VkBuffer buffer, VmaAllocation allocation);
};
