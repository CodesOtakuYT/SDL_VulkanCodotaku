#pragma once
#include <volk.h>
#include <string>
#include <vector>
#include <cstdint>

struct ShaderCompiler {
    void init();
    void shutdown();

    std::vector<uint32_t> compileGLSLToSPIRV(const std::string& source,
                                              VkShaderStageFlagBits stage,
                                              const std::string& filename = "shader.glsl") const;

    std::vector<uint32_t> loadSPIRVFile(const std::string& path) const;

private:
    struct Impl;
    Impl* impl = nullptr;
};

struct ShaderModule {
    VkShaderModule module = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirv;

    ~ShaderModule();
    ShaderModule() = default;
    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;
    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    void createFromSPIRV(VkDevice device, const std::vector<uint32_t>& spirv, VkShaderStageFlagBits stage);
    void createFromGLSL(VkDevice device, const ShaderCompiler& compiler,
                        const std::string& source, VkShaderStageFlagBits stage,
                        const std::string& filename = "shader.glsl");

    VkPipelineShaderStageCreateInfo stageCreateInfo(const char* entryPoint = "main") const;
    bool isValid() const { return module != VK_NULL_HANDLE; }
};
