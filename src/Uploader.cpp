#include "Uploader.h"
#include "Context.h"
#include "Allocator.h"
#include "VkError.h"
#include <cstring>

static VkDeviceSize align4(VkDeviceSize value) {
    return (value + 3) & ~VkDeviceSize(3);
}

void Uploader::init(VulkanContext& context, VulkanAllocator& allocator) {
    ctx = &context;
    alloc = &allocator;
}

void Uploader::destroy() {
    entries.clear();
    stagingOffset = 0;
    ctx = nullptr;
    alloc = nullptr;
}

void Uploader::add(const void* srcData, VkDeviceSize size, VkBuffer dstBuffer, VkDeviceSize dstOffset) {
    VkDeviceSize paddedSize = align4(size);
    VkDeviceSize srcOffset = stagingOffset;

    entries.push_back({srcData, size, srcOffset, dstBuffer, dstOffset});
    stagingOffset += paddedSize;
}

void Uploader::upload() {
    if (entries.empty()) return;

    VkDevice device = ctx->device;
    VkQueue queue = ctx->graphicsQueue;

    // Create one-shot command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = ctx->graphicsQueueFamily;

    VkCommandPool cmdPool;
    vkCheck(vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool), "Uploader: vkCreateCommandPool failed");

    // Allocate one-shot command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkCheck(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "Uploader: vkAllocateCommandBuffers failed");

    // Create staging buffer
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = stagingOffset;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo vmaInfo{};
    vmaInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    alloc->createBuffer(stagingInfo, vmaInfo, &stagingBuffer, &stagingAllocation);

    // Map and copy data into staging buffer
    void* mapped;
    alloc->mapMemory(stagingAllocation, &mapped);

    for (auto& entry : entries) {
        memcpy(static_cast<char*>(mapped) + entry.srcOffset, entry.srcData, entry.size);
    }

    alloc->unmapMemory(stagingAllocation);

    // Begin one-shot command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Record copy commands
    for (auto& entry : entries) {
        VkBufferCopy copy{};
        copy.srcOffset = entry.srcOffset;
        copy.dstOffset = entry.dstOffset;
        copy.size = entry.size;
        vkCmdCopyBuffer(cmd, stagingBuffer, entry.dstBuffer, 1, &copy);
    }

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkCheck(vkCreateFence(device, &fenceInfo, nullptr, &fence), "Uploader: vkCreateFence failed");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkCheck(vkQueueSubmit(queue, 1, &submitInfo, fence), "Uploader: vkQueueSubmit failed");
    vkCheck(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "Uploader: vkWaitForFences failed");

    // Cleanup
    vkDestroyFence(device, fence, nullptr);
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    alloc->destroyBuffer(stagingBuffer, stagingAllocation);

    entries.clear();
    stagingOffset = 0;
}
