#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>

struct VulkanAllocator;

struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator vma = VK_NULL_HANDLE;
    VkDeviceSize size = 0;

    ~Buffer();
    Buffer() = default;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    static Buffer create(VulkanAllocator& alloc, const VkBufferCreateInfo& bufInfo,
                         const VmaAllocationCreateInfo& vmaInfo);
    static Buffer wrap(VkBuffer buffer, VmaAllocation allocation = VK_NULL_HANDLE,
                       VkDeviceSize size = 0);

    VkDeviceAddress getAddress() const;
    bool isValid() const { return handle != VK_NULL_HANDLE; }
};

struct Image {
    VkImage handle = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator vma = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {0, 0, 0};
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags usage = 0;

    ~Image();
    Image() = default;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    static Image create(VulkanAllocator& alloc, const VkImageCreateInfo& imgInfo,
                        const VmaAllocationCreateInfo& vmaInfo,
                        const VkImageViewCreateInfo* viewInfo = nullptr);
    static Image wrap(VkDevice device, VkImage image, VkImageView view,
                      VkFormat format, VkExtent3D extent);
    void recreate(VkExtent3D newExtent);

    bool isValid() const { return handle != VK_NULL_HANDLE; }
};
