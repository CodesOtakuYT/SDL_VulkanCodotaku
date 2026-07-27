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
    VkShaderStageFlagBits stage;
    std::vector<uint32_t> spirv;

    void createFromSPIRV(VkDevice device, const std::vector<uint32_t>& spirv, VkShaderStageFlagBits stage);
    void createFromGLSL(VkDevice device, const ShaderCompiler& compiler,
                        const std::string& source, VkShaderStageFlagBits stage,
                        const std::string& filename = "shader.glsl");
    void destroy(VkDevice device);

    VkPipelineShaderStageCreateInfo stageCreateInfo(const char* entryPoint = "main") const;
};
