#include "Shader.h"
#include "Reflection.h"
#include <shaderc/shaderc.hpp>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>

struct ShaderCompiler::Impl {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    Impl() {
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
    }
};

bool ShaderCompiler::init() {
    impl = new Impl();
    if (!impl->compiler.IsValid()) {
        fprintf(stderr, "shaderc::Compiler initialization failed\n");
        delete impl;
        impl = nullptr;
        return false;
    }
    return true;
}

void ShaderCompiler::shutdown() {
    delete impl;
    impl = nullptr;
}

std::vector<uint32_t> ShaderCompiler::compileGLSLToSPIRV(const std::string& source,
                                                          VkShaderStageFlagBits stage,
                                                          const std::string& filename) const {
    if (!impl) return {};

    shaderc_shader_kind kind;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   kind = shaderc_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  kind = shaderc_compute_shader; break;
        default:
            fprintf(stderr, "Unsupported shader stage: %d\n", stage);
            return {};
    }

    auto result = impl->compiler.CompileGlslToSpv(source, kind, filename.c_str(), impl->options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fprintf(stderr, "Shader compilation failed (%s):\n%s\n",
                filename.c_str(), result.GetErrorMessage().c_str());
        return {};
    }

    return {result.cbegin(), result.cend()};
}

std::vector<uint32_t> ShaderCompiler::loadSPIRVFile(const std::string& path) const {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open SPIR-V file: %s\n", path.c_str());
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        fprintf(stderr, "Invalid SPIR-V file size: %s\n", path.c_str());
        return {};
    }

    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return buffer;
}

bool ShaderModule::createFromSPIRV(VkDevice device, const std::vector<uint32_t>& spirvData, VkShaderStageFlagBits shaderStage) {
    if (spirvData.empty()) return false;

    spirv = spirvData;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create shader module\n");
        return false;
    }

    stage = shaderStage;

    reflectAndPrint(spirv, stage);

    return true;
}

bool ShaderModule::createFromGLSL(VkDevice device, const ShaderCompiler& compiler,
                                   const std::string& source, VkShaderStageFlagBits shaderStage,
                                   const std::string& filename) {
    auto spirvData = compiler.compileGLSLToSPIRV(source, shaderStage, filename);
    return createFromSPIRV(device, spirvData, shaderStage);
}

void ShaderModule::destroy(VkDevice device) {
    if (module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module, nullptr);
        module = VK_NULL_HANDLE;
    }
}

VkPipelineShaderStageCreateInfo ShaderModule::stageCreateInfo(const char* entryPoint) const {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = module;
    info.pName = entryPoint;
    return info;
}
