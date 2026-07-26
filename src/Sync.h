#pragma once
#include <volk.h>

// Sync helpers for VK_KHR_unified_image_layouts.
// With this extension, VK_IMAGE_LAYOUT_GENERAL is efficient everywhere
// except for image creation (UNDEFINED) and presentation (PRESENT_SRC_KHR).

namespace sync {

inline void imageBarrier(VkCommandBuffer cmd,
                          VkImage image,
                          VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                          VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
                          VkImageLayout oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                          VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL,
                          uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                          uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

inline void imageBarrierDepth(VkCommandBuffer cmd,
                               VkImage image,
                               VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                               VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
                               VkImageLayout oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                               VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL,
                               uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                               uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

// Discard image contents and transition to GENERAL (first use after creation or UNDEFINED)
inline void discardToGeneral(VkCommandBuffer cmd, VkImage image) {
    imageBarrier(cmd, image,
                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

// Transition to PRESENT_SRC_KHR (required for presentation)
inline void toPresent(VkCommandBuffer cmd, VkImage image) {
    imageBarrier(cmd, image,
                 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0,
                 VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

// Memory write -> shader read (same layout, just access synchronization)
inline void memoryWriteToShaderRead(VkCommandBuffer cmd, VkImage image) {
    imageBarrier(cmd, image,
                 VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

// Shader write -> shader read (storage image read-after-write)
inline void shaderWriteToShaderRead(VkCommandBuffer cmd, VkImage image) {
    imageBarrier(cmd, image,
                 VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

} // namespace sync
