#pragma once
#include "Reflection.h"
#include <volk.h>
#include <vector>
#include <string>

inline VkShaderStageFlagBits inferShaderStage(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) {
        fprintf(stderr, "Cannot infer shader stage from filename (no extension): %s\n", filename.c_str());
        return VK_SHADER_STAGE_VERTEX_BIT;
    }
    std::string ext = filename.substr(dot);
    if (ext == ".vert") return VK_SHADER_STAGE_VERTEX_BIT;
    if (ext == ".frag") return VK_SHADER_STAGE_FRAGMENT_BIT;
    if (ext == ".comp") return VK_SHADER_STAGE_COMPUTE_BIT;
    if (ext == ".geom") return VK_SHADER_STAGE_GEOMETRY_BIT;
    if (ext == ".tesc") return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (ext == ".tese") return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    fprintf(stderr, "Unknown shader extension '%s' for file: %s\n", ext.c_str(), filename.c_str());
    return VK_SHADER_STAGE_VERTEX_BIT;
}

inline uint32_t formatSize(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R32_SFLOAT: return 4;
        case VK_FORMAT_R32G32_SFLOAT: return 8;
        case VK_FORMAT_R32G32B32_SFLOAT: return 12;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return 16;
        case VK_FORMAT_R32G32_SINT: return 8;
        case VK_FORMAT_R32G32B32_SINT: return 12;
        case VK_FORMAT_R32G32B32A32_SINT: return 16;
        case VK_FORMAT_R32G32_UINT: return 8;
        case VK_FORMAT_R32G32B32_UINT: return 12;
        case VK_FORMAT_R32G32B32A32_UINT: return 16;
        case VK_FORMAT_R8G8B8A8_UNORM: return 4;
        case VK_FORMAT_R8G8B8A8_SNORM: return 4;
        case VK_FORMAT_R8G8B8A8_UINT: return 4;
        case VK_FORMAT_R8G8B8A8_SRGB: return 4;
        case VK_FORMAT_B8G8R8A8_UNORM: return 4;
        case VK_FORMAT_B8G8R8A8_SRGB: return 4;
        case VK_FORMAT_R16G16_SFLOAT: return 4;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
        default: return 4;
    }
}

struct PipelineConfig {
    struct ShaderConfig {
        std::vector<uint32_t> spirv;
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
        std::string filename;
    };
    std::vector<ShaderConfig> shaders;
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkFormat colorAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
};

struct Pipeline {
    VkPipeline handle = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> setLayouts;
    MergedReflection reflection;

    static Pipeline create(VkDevice device, const PipelineConfig& config);
    void destroy(VkDevice device);
};
