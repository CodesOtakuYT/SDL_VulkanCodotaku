#pragma once
#include <spirv_reflect.h>
#include <volk.h>
#include <vector>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstring>

inline const char* reflectDescriptorType(SpvReflectDescriptorType type) {
    switch (type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return "Sampler";
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "CombinedImageSampler";
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "SampledImage";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "StorageImage";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "UniformBuffer";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "StorageBuffer";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "UniformTexelBuffer";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "StorageTexelBuffer";
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "InputAttachment";
        default: return "Unknown";
    }
}

struct MergedReflection {
    struct DescriptorBinding {
        uint32_t set = 0;
        uint32_t binding = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uint32_t count = 1;
        VkShaderStageFlags stageFlags = 0;
    };

    struct PushConstantRange {
        VkShaderStageFlags stageFlags = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    struct VertexInput {
        uint32_t location = 0;
        uint32_t binding = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t offset = 0;
    };

    std::vector<DescriptorBinding> descriptors;
    std::vector<PushConstantRange> pushConstants;
    std::vector<VertexInput> vertexInputs;
    uint32_t highestSet = 0;

    static MergedReflection merge(const std::vector<std::pair<std::vector<uint32_t>, VkShaderStageFlagBits>>& shaders) {
        MergedReflection merged;

        for (auto& [spirv, stage] : shaders) {
            SpvReflectShaderModule module{};
            SpvReflectResult result = spvReflectCreateShaderModule(
                spirv.size() * sizeof(uint32_t), spirv.data(), &module);
            if (result != SPV_REFLECT_RESULT_SUCCESS) {
                fprintf(stderr, "SPIRV-Reflect failed: %d\n", result);
                continue;
            }

            // --- Descriptor bindings ---
            uint32_t descCount = 0;
            spvReflectEnumerateDescriptorBindings(&module, &descCount, nullptr);
            if (descCount > 0) {
                std::vector<SpvReflectDescriptorBinding*> bindings(descCount);
                spvReflectEnumerateDescriptorBindings(&module, &descCount, bindings.data());

                for (auto* b : bindings) {
                    // Find existing or add new
                    auto it = std::find_if(merged.descriptors.begin(), merged.descriptors.end(),
                        [&](const DescriptorBinding& d) {
                            return d.set == b->set && d.binding == b->binding;
                        });

                    if (it != merged.descriptors.end()) {
                        it->stageFlags |= stage;
                    } else {
                        DescriptorBinding desc{};
                        desc.set = b->set;
                        desc.binding = b->binding;
                        desc.type = static_cast<VkDescriptorType>(b->descriptor_type);
                        desc.count = b->count;
                        desc.stageFlags = stage;
                        merged.descriptors.push_back(desc);
                    }
                    if (b->set > merged.highestSet) merged.highestSet = b->set;
                }
            }

            // --- Push constants ---
            uint32_t pcCount = 0;
            spvReflectEnumeratePushConstantBlocks(&module, &pcCount, nullptr);
            if (pcCount > 0) {
                std::vector<SpvReflectBlockVariable*> pcs(pcCount);
                spvReflectEnumeratePushConstantBlocks(&module, &pcCount, pcs.data());

                for (auto* pc : pcs) {
                    // Find existing overlapping range or add new
                    auto it = std::find_if(merged.pushConstants.begin(), merged.pushConstants.end(),
                        [&](const PushConstantRange& r) {
                            return r.offset == pc->offset && r.size == pc->size;
                        });

                    if (it != merged.pushConstants.end()) {
                        it->stageFlags |= stage;
                    } else {
                        PushConstantRange range{};
                        range.stageFlags = stage;
                        range.offset = pc->offset;
                        range.size = pc->size;
                        merged.pushConstants.push_back(range);
                    }
                }
            }

            // --- Vertex inputs (vertex stage only) ---
            if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
                uint32_t inputCount = 0;
                spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);
                if (inputCount > 0) {
                    std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
                    spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());

                    for (auto* v : inputs) {
                        if (v->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) continue;

                        VertexInput vi{};
                        vi.location = v->location;
                        vi.binding = 0; // all attributes share binding 0 (interleaved)
                        vi.format = static_cast<VkFormat>(v->format);
                        vi.offset = 0; // computed below from accumulated sizes
                        merged.vertexInputs.push_back(vi);
                    }
                    std::sort(merged.vertexInputs.begin(), merged.vertexInputs.end(),
                        [](const VertexInput& a, const VertexInput& b) { return a.location < b.location; });
                }
            }

            spvReflectDestroyShaderModule(&module);
        }

        return merged;
    }

    void print() const {
        fprintf(stderr, "\n=== Merged Reflection ===\n");

        if (!descriptors.empty()) {
            fprintf(stderr, "  Descriptor Bindings (%zu):\n", descriptors.size());
            for (auto& d : descriptors) {
                fprintf(stderr, "    set=%u binding=%u type=%s count=%u stages=0x%x\n",
                       d.set, d.binding, reflectDescriptorType(static_cast<SpvReflectDescriptorType>(d.type)), d.count, d.stageFlags);
            }
        } else {
            fprintf(stderr, "  Descriptor Bindings: none\n");
        }

        if (!pushConstants.empty()) {
            fprintf(stderr, "  Push Constants (%zu):\n", pushConstants.size());
            for (auto& pc : pushConstants) {
                fprintf(stderr, "    offset=%u size=%u stages=0x%x\n", pc.offset, pc.size, pc.stageFlags);
            }
        } else {
            fprintf(stderr, "  Push Constants: none\n");
        }

        if (!vertexInputs.empty()) {
            fprintf(stderr, "  Vertex Inputs (%zu):\n", vertexInputs.size());
            for (auto& vi : vertexInputs) {
                fprintf(stderr, "    location=%u binding=%u format=%d offset=%u\n",
                       vi.location, vi.binding, vi.format, vi.offset);
            }
        } else {
            fprintf(stderr, "  Vertex Inputs: none\n");
        }

        fprintf(stderr, "  Highest descriptor set: %u\n", highestSet);
    }
};

inline void reflectAndPrint(const std::vector<uint32_t>& spirv, VkShaderStageFlagBits stage) {
    SpvReflectShaderModule module{};
    SpvReflectResult result = spvReflectCreateShaderModule(
        spirv.size() * sizeof(uint32_t), spirv.data(), &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        fprintf(stderr, "SPIRV-Reflect failed with error %d\n", result);
        return;
    }

    const char* stageName = "";
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: stageName = "Vertex"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: stageName = "Fragment"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: stageName = "Compute"; break;
        default: stageName = "Unknown"; break;
    }

    fprintf(stderr, "\n=== SPIRV-Reflect: %s Shader ===\n", stageName);

    uint32_t descCount = 0;
    spvReflectEnumerateDescriptorBindings(&module, &descCount, nullptr);
    if (descCount > 0) {
        std::vector<SpvReflectDescriptorBinding*> bindings(descCount);
        spvReflectEnumerateDescriptorBindings(&module, &descCount, bindings.data());
        fprintf(stderr, "  Descriptor Bindings (%u):\n", descCount);
        for (auto* b : bindings) {
            fprintf(stderr, "    set=%u binding=%u type=%s name=\"%s\"\n",
                   b->set, b->binding, reflectDescriptorType(b->descriptor_type), b->name);
        }
    } else {
        fprintf(stderr, "  Descriptor Bindings: none\n");
    }

    uint32_t pcCount = 0;
    spvReflectEnumeratePushConstantBlocks(&module, &pcCount, nullptr);
    if (pcCount > 0) {
        std::vector<SpvReflectBlockVariable*> pcs(pcCount);
        spvReflectEnumeratePushConstantBlocks(&module, &pcCount, pcs.data());
        fprintf(stderr, "  Push Constants (%u):\n", pcCount);
        for (auto* pc : pcs) {
            fprintf(stderr, "    offset=%u size=%u\n", pc->offset, pc->size);
        }
    } else {
        fprintf(stderr, "  Push Constants: none\n");
    }

    uint32_t inputCount = 0;
    spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);
    if (inputCount > 0) {
        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
        spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());
        fprintf(stderr, "  Input Variables (%u):\n", inputCount);
        for (auto* v : inputs) {
            fprintf(stderr, "    location=%u name=\"%s\"\n", v->location, v->name ? v->name : "(null)");
        }
    }

    uint32_t outputCount = 0;
    spvReflectEnumerateOutputVariables(&module, &outputCount, nullptr);
    if (outputCount > 0) {
        std::vector<SpvReflectInterfaceVariable*> outputs(outputCount);
        spvReflectEnumerateOutputVariables(&module, &outputCount, outputs.data());
        fprintf(stderr, "  Output Variables (%u):\n", outputCount);
        for (auto* v : outputs) {
            fprintf(stderr, "    location=%u name=\"%s\"\n", v->location, v->name ? v->name : "(null)");
        }
    }

    spvReflectDestroyShaderModule(&module);
}
