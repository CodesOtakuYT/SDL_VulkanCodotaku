#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct VulkanAllocator;

struct GBufferEntry {
    std::string name;
    VkFormat format;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageUsageFlags usage;
};

class GBuffer {
public:
    void init(VkDevice device, VulkanAllocator& alloc, glm::uvec2 extent);
    uint32_t add(const GBufferEntry& entry);
    void resize(glm::uvec2 extent);
    void destroy();

    VkImage getImage(uint32_t handle) const;
    VkImageView getView(uint32_t handle) const;
    VkFormat getFormat(uint32_t handle) const;
    VkSampleCountFlagBits getSamples(uint32_t handle) const;
    glm::uvec2 getExtent() const;

private:
    struct Image {
        GBufferEntry config;
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    void createImage(Image& img);
    void destroyImage(Image& img);
    VkImageAspectFlags aspectFromUsage(VkImageUsageFlags usage) const;

    VkDevice device = VK_NULL_HANDLE;
    VulkanAllocator* alloc = nullptr;
    glm::uvec2 extent{0, 0};
    std::vector<Image> images;
};
