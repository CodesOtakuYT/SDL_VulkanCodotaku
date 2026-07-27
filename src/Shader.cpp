#include "Shader.h"
#include "Reflection.h"
#include "VkError.h"
#include <shaderc/shaderc.hpp>
#include <fstream>
#include <sstream>

struct ShaderCompiler::Impl {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    Impl() {
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
    }
};

void ShaderCompiler::init() {
    impl = new Impl();
    if (!impl->compiler.IsValid()) {
        delete impl;
        impl = nullptr;
        throw VkbError("shaderc::Compiler initialization failed");
    }
}

void ShaderCompiler::shutdown() {
    delete impl;
    impl = nullptr;
}

std::vector<uint32_t> ShaderCompiler::compileGLSLToSPIRV(const std::string& source,
                                                          VkShaderStageFlagBits stage,
                                                          const std::string& filename) const {
    if (!impl) throw VkbError("ShaderCompiler not initialized");

    shaderc_shader_kind kind;
    switch (stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:   kind = shaderc_vertex_shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: kind = shaderc_fragment_shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:  kind = shaderc_compute_shader; break;
        default:
            throw VkbError("Unsupported shader stage");
    }

    auto result = impl->compiler.CompileGlslToSpv(source, kind, filename.c_str(), impl->options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw VkbError(result.GetErrorMessage().c_str());
    }

    return {result.cbegin(), result.cend()};
}

std::vector<uint32_t> ShaderCompiler::loadSPIRVFile(const std::string& path) const {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw VkbError("Failed to open SPIR-V file");
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        throw VkbError("Invalid SPIR-V file size");
    }

    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return buffer;
}

void ShaderModule::createFromSPIRV(VkDevice dev, const std::vector<uint32_t>& spirvData, VkShaderStageFlagBits shaderStage) {
    if (spirvData.empty()) throw VkbError("SPIR-V data is empty");

    spirv = spirvData;
    device = dev;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    vkCheck(vkCreateShaderModule(dev, &createInfo, nullptr, &module), "vkCreateShaderModule failed");
    stage = shaderStage;

    reflectAndPrint(spirv, stage);
}

void ShaderModule::createFromGLSL(VkDevice dev, const ShaderCompiler& compiler,
                                   const std::string& source, VkShaderStageFlagBits shaderStage,
                                   const std::string& filename) {
    auto spirvData = compiler.compileGLSLToSPIRV(source, shaderStage, filename);
    createFromSPIRV(dev, spirvData, shaderStage);
}

ShaderModule::~ShaderModule() {
    if (module != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, module, nullptr);
    }
}

ShaderModule::ShaderModule(ShaderModule&& other) noexcept
    : module(other.module), device(other.device), stage(other.stage), spirv(std::move(other.spirv)) {
    other.module = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept {
    if (this != &other) {
        if (module != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, module, nullptr);
        }
        module = other.module;
        device = other.device;
        stage = other.stage;
        spirv = std::move(other.spirv);
        other.module = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
    }
    return *this;
}

VkPipelineShaderStageCreateInfo ShaderModule::stageCreateInfo(const char* entryPoint) const {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage;
    info.module = module;
    info.pName = entryPoint;
    return info;
}
