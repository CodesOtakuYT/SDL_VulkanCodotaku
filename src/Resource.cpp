#include "Resource.h"
#include "Allocator.h"
#include "VkError.h"

// --- Buffer ---

Buffer::~Buffer() {
    if (vma != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(vma, handle, allocation);
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : handle(other.handle), allocation(other.allocation), device(other.device), vma(other.vma), size(other.size) {
    other.handle = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
    other.vma = VK_NULL_HANDLE;
    other.size = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (vma != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(vma, handle, allocation);
        }
        handle = other.handle;
        allocation = other.allocation;
        device = other.device;
        vma = other.vma;
        size = other.size;
        other.handle = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
        other.vma = VK_NULL_HANDLE;
        other.size = 0;
    }
    return *this;
}

Buffer Buffer::create(VulkanAllocator& alloc, const VkBufferCreateInfo& bufInfo,
                      const VmaAllocationCreateInfo& vmaInfo) {
    Buffer buf;
    buf.size = bufInfo.size;
    buf.device = alloc.device;
    buf.vma = alloc.allocator;
    vkCheck(vmaCreateBuffer(alloc.allocator, &bufInfo, &vmaInfo, &buf.handle, &buf.allocation, nullptr),
            "Buffer::create failed");
    return buf;
}

Buffer Buffer::wrap(VkBuffer buffer, VmaAllocation allocation, VkDeviceSize size) {
    Buffer buf;
    buf.handle = buffer;
    buf.allocation = allocation;
    buf.size = size;
    return buf;
}

VkDeviceAddress Buffer::getAddress() const {
    if (handle == VK_NULL_HANDLE) return 0;
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = handle;
    return vkGetBufferDeviceAddress(device, &addrInfo);
}

// --- Image ---

Image::~Image() {
    if (device != VK_NULL_HANDLE) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
        if (vma != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(vma, handle, allocation);
        }
    }
}

Image::Image(Image&& other) noexcept
    : handle(other.handle), view(other.view), allocation(other.allocation),
      device(other.device), vma(other.vma),
      format(other.format), extent(other.extent), samples(other.samples), usage(other.usage) {
    other.handle = VK_NULL_HANDLE;
    other.view = VK_NULL_HANDLE;
    other.allocation = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
    other.vma = VK_NULL_HANDLE;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        if (device != VK_NULL_HANDLE) {
            if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
            if (vma != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) vmaDestroyImage(vma, handle, allocation);
        }
        handle = other.handle;
        view = other.view;
        allocation = other.allocation;
        device = other.device;
        vma = other.vma;
        format = other.format;
        extent = other.extent;
        samples = other.samples;
        usage = other.usage;
        other.handle = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
        other.vma = VK_NULL_HANDLE;
    }
    return *this;
}

Image Image::create(VulkanAllocator& alloc, const VkImageCreateInfo& imgInfo,
                    const VmaAllocationCreateInfo& vmaInfo, const VkImageViewCreateInfo* viewInfo) {
    Image img;
    img.device = alloc.device;
    img.vma = alloc.allocator;
    img.format = imgInfo.format;
    img.extent = imgInfo.extent;
    img.samples = imgInfo.samples;
    img.usage = imgInfo.usage;

    vkCheck(vmaCreateImage(alloc.allocator, &imgInfo, &vmaInfo, &img.handle, &img.allocation, nullptr),
            "Image::create failed");

    if (viewInfo) {
        VkImageViewCreateInfo vi = *viewInfo;
        vi.image = img.handle;
        vkCheck(vkCreateImageView(alloc.device, &vi, nullptr, &img.view),
                "Image::create imageView failed");
    }

    return img;
}

Image Image::wrap(VkDevice dev, VkImage image, VkImageView view, VkFormat format, VkExtent3D extent) {
    Image img;
    img.handle = image;
    img.view = view;
    img.device = dev;
    img.format = format;
    img.extent = extent;
    return img;
}

void Image::recreate(VkExtent3D newExtent) {
    if (device != VK_NULL_HANDLE) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
            view = VK_NULL_HANDLE;
        }
        if (vma != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(vma, handle, allocation);
            handle = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }
    }

    extent = newExtent;

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent = extent;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = samples;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = usage;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaAllocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkCheck(vmaCreateImage(vma, &imgInfo, &vmaAllocInfo, &handle, &allocation, nullptr),
            "Image::recreate failed");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = handle;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = (format == VK_FORMAT_D32_SFLOAT ||
                                            format == VK_FORMAT_D24_UNORM_S8_UINT ||
                                            format == VK_FORMAT_D32_SFLOAT_S8_UINT)
                                               ? VK_IMAGE_ASPECT_DEPTH_BIT
                                               : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    vkCheck(vkCreateImageView(device, &viewInfo, nullptr, &view),
            "Image::recreate imageView failed");
}
