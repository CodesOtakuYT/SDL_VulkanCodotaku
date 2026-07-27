#pragma once
#include <spirv_reflect.h>
#include <volk.h>
#include <vector>
#include <cstdio>

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

inline const char* reflectStorageClass(SpvStorageClass sc) {
    switch (sc) {
        case SpvStorageClassInput: return "Input";
        case SpvStorageClassOutput: return "Output";
        case SpvStorageClassUniformConstant: return "UniformConstant";
        case SpvStorageClassUniform: return "Uniform";
        case SpvStorageClassStorageBuffer: return "StorageBuffer";
        default: return "Other";
    }
}

inline void printShaderReflection(SpvReflectShaderModule& module, VkShaderStageFlagBits stage) {
    const char* stageName = "";
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: stageName = "Vertex"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: stageName = "Fragment"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: stageName = "Compute"; break;
        default: stageName = "Unknown"; break;
    }

    fprintf(stderr, "\n=== SPIRV-Reflect: %s Shader ===\n", stageName);

    // Descriptor bindings
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

    // Descriptor sets
    uint32_t setCount = 0;
    spvReflectEnumerateDescriptorSets(&module, &setCount, nullptr);
    if (setCount > 0) {
        std::vector<SpvReflectDescriptorSet*> sets(setCount);
        spvReflectEnumerateDescriptorSets(&module, &setCount, sets.data());

        fprintf(stderr, "  Descriptor Sets (%u):\n", setCount);
        for (auto* s : sets) {
            fprintf(stderr, "    set=%u bindings=%u\n", s->set, s->binding_count);
        }
    }

    // Push constants
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

    // Input variables (vertex attributes / fragment inputs)
    uint32_t inputCount = 0;
    spvReflectEnumerateInputVariables(&module, &inputCount, nullptr);
    if (inputCount > 0) {
        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
        spvReflectEnumerateInputVariables(&module, &inputCount, inputs.data());
        fprintf(stderr, "  Input Variables (%u):\n", inputCount);
        for (auto* v : inputs) {
            fprintf(stderr, "    location=%u name=\"%s\"\n", v->location, v->name);
        }
    }

    // Output variables
    uint32_t outputCount = 0;
    spvReflectEnumerateOutputVariables(&module, &outputCount, nullptr);
    if (outputCount > 0) {
        std::vector<SpvReflectInterfaceVariable*> outputs(outputCount);
        spvReflectEnumerateOutputVariables(&module, &outputCount, outputs.data());
        fprintf(stderr, "  Output Variables (%u):\n", outputCount);
        for (auto* v : outputs) {
            fprintf(stderr, "    location=%u name=\"%s\"\n", v->location, v->name);
        }
    }
}

inline void reflectAndPrint(const std::vector<uint32_t>& spirv, VkShaderStageFlagBits stage) {
    SpvReflectShaderModule module{};
    SpvReflectResult result = spvReflectCreateShaderModule(
        spirv.size() * sizeof(uint32_t), spirv.data(), &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        fprintf(stderr, "SPIRV-Reflect failed with error %d\n", result);
        return;
    }

    printShaderReflection(module, stage);
    spvReflectDestroyShaderModule(&module);
}
