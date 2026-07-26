#define VMA_IMPLEMENTATION
#include "Allocator.h"
#include <cstdio>

bool VulkanAllocator::init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice dev) {
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo{};
    createInfo.instance = instance;
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = dev;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_4;
    createInfo.pVulkanFunctions = &vulkanFunctions;
    createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VkResult result = vmaCreateAllocator(&createInfo, &allocator);
    if (result == VK_SUCCESS) {
        this->device = dev;
    }
    return result == VK_SUCCESS;
}

void VulkanAllocator::shutdown() {
    if (allocator != VK_NULL_HANDLE) {
        VmaTotalStatistics stats;
        vmaCalculateStatistics(allocator, &stats);
        if (stats.total.statistics.allocationBytes > 0) {
            fprintf(stderr, "VMA: %llu bytes still allocated at shutdown\n",
                    stats.total.statistics.allocationBytes);
        }
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
    }
}

VkResult VulkanAllocator::createBuffer(const VkBufferCreateInfo& bufferInfo,
                                       const VmaAllocationCreateInfo& allocInfo,
                                       VkBuffer* buffer,
                                       VmaAllocation* allocation,
                                       VmaAllocationInfo* allocInfoOut) {
    return vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, buffer, allocation, allocInfoOut);
}

void VulkanAllocator::destroyBuffer(VkBuffer buffer, VmaAllocation allocation) {
    vmaDestroyBuffer(allocator, buffer, allocation);
}

VkResult VulkanAllocator::mapMemory(VmaAllocation allocation, void** data) {
    return vmaMapMemory(allocator, allocation, data);
}

void VulkanAllocator::unmapMemory(VmaAllocation allocation) {
    vmaUnmapMemory(allocator, allocation);
}

VkDeviceSize VulkanAllocator::getBufferDeviceAddress(VkBuffer buffer, VmaAllocation allocation) {
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;
    return vkGetBufferDeviceAddress(device, &addressInfo);
}
