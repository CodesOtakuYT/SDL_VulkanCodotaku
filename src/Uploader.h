#pragma once
#include <volk.h>
#include <cstdint>
#include <vector>

struct VulkanContext;
struct VulkanAllocator;

struct Uploader {
    void init(VulkanContext& ctx, VulkanAllocator& alloc);
    void destroy();

    void add(const void* srcData, VkDeviceSize size, VkBuffer dstBuffer, VkDeviceSize dstOffset = 0);
    void upload();

private:
    VulkanContext* ctx = nullptr;
    VulkanAllocator* alloc = nullptr;

    struct CopyEntry {
        const void* srcData;
        VkDeviceSize size;
        VkDeviceSize srcOffset;
        VkBuffer dstBuffer;
        VkDeviceSize dstOffset;
    };
    std::vector<CopyEntry> entries;
    VkDeviceSize stagingOffset = 0;
};
