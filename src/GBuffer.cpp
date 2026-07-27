#include "GBuffer.h"
#include "Allocator.h"
#include "VkError.h"

void GBuffer::init(VkDevice dev, VulkanAllocator& allocator, glm::uvec2 ext) {
    device = dev;
    alloc = &allocator;
    extent = ext;
}

uint32_t GBuffer::add(const GBufferEntry& entry) {
    uint32_t handle = static_cast<uint32_t>(images.size());
    images.push_back({entry, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE});
    if (extent.x > 0 && extent.y > 0) {
        createImage(images.back());
    }
    return handle;
}

void GBuffer::resize(glm::uvec2 ext) {
    for (auto& img : images) {
        destroyImage(img);
    }
    extent = ext;
    if (extent.x > 0 && extent.y > 0) {
        for (auto& img : images) {
            createImage(img);
        }
    }
}

void GBuffer::destroy() {
    for (auto& img : images) {
        destroyImage(img);
    }
}

VkImage GBuffer::getImage(uint32_t handle) const { return images[handle].image; }
VkImageView GBuffer::getView(uint32_t handle) const { return images[handle].view; }
VkFormat GBuffer::getFormat(uint32_t handle) const { return images[handle].config.format; }
VkSampleCountFlagBits GBuffer::getSamples(uint32_t handle) const { return images[handle].config.samples; }
glm::uvec2 GBuffer::getExtent() const { return extent; }

void GBuffer::createImage(Image& img) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.x, extent.y, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = img.config.format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.samples = img.config.samples;
    imageInfo.usage = img.config.usage;

    VmaAllocationCreateInfo vmaInfo{};
    vmaInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    vkCheck(vmaCreateImage(alloc->allocator, &imageInfo, &vmaInfo, &img.image, &img.allocation, nullptr),
            "GBuffer: vmaCreateImage failed");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = img.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = img.config.format;
    viewInfo.subresourceRange.aspectMask = aspectFromUsage(img.config.usage);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCheck(vkCreateImageView(device, &viewInfo, nullptr, &img.view),
            "GBuffer: vkCreateImageView failed");

    if (!img.config.name.empty()) {
        vkSetObjectName(device, VK_OBJECT_TYPE_IMAGE, (uint64_t)img.image, img.config.name.c_str());
        vkSetObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)img.view, img.config.name.c_str());
    }
}

void GBuffer::destroyImage(Image& img) {
    if (img.view) { vkDestroyImageView(device, img.view, nullptr); img.view = VK_NULL_HANDLE; }
    if (img.image) { vmaDestroyImage(alloc->allocator, img.image, img.allocation); img.image = VK_NULL_HANDLE; img.allocation = VK_NULL_HANDLE; }
}

VkImageAspectFlags GBuffer::aspectFromUsage(VkImageUsageFlags usage) const {
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}
