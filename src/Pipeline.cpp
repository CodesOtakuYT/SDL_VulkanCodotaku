#include "Pipeline.h"
#include "VkError.h"
#include <algorithm>

Pipeline Pipeline::create(VkDevice device, const PipelineConfig& config) {
    Pipeline pipeline;
    pipeline.device = device;

    // --- 1. Merge reflections from all shaders ---
    std::vector<std::pair<std::vector<uint32_t>, VkShaderStageFlagBits>> shaderData;
    for (auto& s : config.shaders) {
        shaderData.push_back({s.spirv, s.stage});
    }
    pipeline.reflection = MergedReflection::merge(shaderData);
    pipeline.reflection.print();

    // --- 2. Compute per-set layout sizes ---
    uint32_t setCount = pipeline.reflection.highestSet + 1;
    std::vector<uint32_t> bindingsPerSet(setCount, 0);
    for (auto& d : pipeline.reflection.descriptors) {
        bindingsPerSet[d.set]++;
    }

    // --- 3. Create VkDescriptorSetLayout per set ---
    pipeline.setLayouts.resize(setCount);
    for (uint32_t s = 0; s < setCount; s++) {
        if (bindingsPerSet[s] == 0) {
            pipeline.setLayouts[s] = VK_NULL_HANDLE;
            continue;
        }

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(bindingsPerSet[s]);

        for (auto& d : pipeline.reflection.descriptors) {
            if (d.set != s) continue;

            VkDescriptorSetLayoutBinding b{};
            b.binding = d.binding;
            b.descriptorType = d.type;
            b.descriptorCount = d.count;
            b.stageFlags = d.stageFlags;
            bindings.push_back(b);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        vkCheck(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &pipeline.setLayouts[s]),
                "vkCreateDescriptorSetLayout failed");
    }

    // --- 4. Create VkPipelineLayout ---
    std::vector<VkPushConstantRange> pcRanges;
    for (auto& pc : pipeline.reflection.pushConstants) {
        VkPushConstantRange range{};
        range.stageFlags = pc.stageFlags;
        range.offset = pc.offset;
        range.size = pc.size;
        pcRanges.push_back(range);
    }

    // Filter out null set layouts
    std::vector<VkDescriptorSetLayout> validLayouts;
    for (auto& l : pipeline.setLayouts) {
        if (l != VK_NULL_HANDLE) validLayouts.push_back(l);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(validLayouts.size());
    pipelineLayoutInfo.pSetLayouts = validLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pcRanges.data();

    vkCheck(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline.layout),
            "vkCreatePipelineLayout failed");

    // --- 5. Build vertex input state from reflection ---
    auto& vertInputs = pipeline.reflection.vertexInputs;

    // Compute offsets by accumulating format sizes
    uint32_t runningOffset = 0;
    for (auto& vi : vertInputs) {
        vi.offset = runningOffset;
        runningOffset += formatSize(vi.format);
    }

    std::vector<VkVertexInputAttributeDescription> attrs(vertInputs.size());
    for (size_t i = 0; i < vertInputs.size(); i++) {
        attrs[i].location = vertInputs[i].location;
        attrs[i].binding = vertInputs[i].binding;
        attrs[i].format = vertInputs[i].format;
        attrs[i].offset = vertInputs[i].offset;
    }

    // Collect unique bindings
    std::vector<uint32_t> uniqueBindings;
    for (auto& vi : vertInputs) {
        if (std::find(uniqueBindings.begin(), uniqueBindings.end(), vi.binding) == uniqueBindings.end()) {
            uniqueBindings.push_back(vi.binding);
        }
    }

    std::vector<VkVertexInputBindingDescription> bindingsDesc(uniqueBindings.size());
    for (size_t i = 0; i < uniqueBindings.size(); i++) {
        bindingsDesc[i].binding = uniqueBindings[i];
        bindingsDesc[i].stride = runningOffset; // interleaved
        bindingsDesc[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (!attrs.empty()) {
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingsDesc.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingsDesc.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attrs.data();
    }

    // --- 6. Build remaining pipeline state ---
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = config.samples;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(config.dynamicStates.size());
    dynamicState.pDynamicStates = config.dynamicStates.data();

    // Dynamic rendering
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &config.colorAttachmentFormat;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    if (config.depthAttachmentFormat != VK_FORMAT_UNDEFINED) {
        renderingInfo.depthAttachmentFormat = config.depthAttachmentFormat;
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    }

    // --- 7. Shader stages ---
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    for (auto& s : config.shaders) {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = s.stage;
        stageInfo.pName = "main";

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = s.spirv.size() * sizeof(uint32_t);
        moduleInfo.pCode = s.spirv.data();

        VkShaderModule shaderModule;
        vkCheck(vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule),
                "vkCreateShaderModule (pipeline stage) failed");
        stageInfo.module = shaderModule;
        stages.push_back(stageInfo);
    }

    // --- 8. Create pipeline cache ---
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkCheck(vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipeline.cache),
            "vkCreatePipelineCache failed");

    // --- 9. Create graphics pipeline ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = pipeline.layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    vkCheck(vkCreateGraphicsPipelines(device, pipeline.cache, 1, &pipelineInfo, nullptr, &pipeline.handle),
            "vkCreateGraphicsPipelines failed");

    // Clean up temporary shader modules
    for (auto& s : stages) {
        vkDestroyShaderModule(device, s.module, nullptr);
    }

    return pipeline;
}

Pipeline::~Pipeline() {
    if (device == VK_NULL_HANDLE) return;
    if (handle != VK_NULL_HANDLE) vkDestroyPipeline(device, handle, nullptr);
    if (cache != VK_NULL_HANDLE) vkDestroyPipelineCache(device, cache, nullptr);
    if (layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout, nullptr);
    for (auto& l : setLayouts) {
        if (l != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, l, nullptr);
    }
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : handle(other.handle), layout(other.layout), cache(other.cache),
      setLayouts(std::move(other.setLayouts)), reflection(other.reflection), device(other.device) {
    other.handle = VK_NULL_HANDLE;
    other.layout = VK_NULL_HANDLE;
    other.cache = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this != &other) {
        if (device != VK_NULL_HANDLE) {
            if (handle != VK_NULL_HANDLE) vkDestroyPipeline(device, handle, nullptr);
            if (cache != VK_NULL_HANDLE) vkDestroyPipelineCache(device, cache, nullptr);
            if (layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, layout, nullptr);
            for (auto& l : setLayouts) {
                if (l != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, l, nullptr);
            }
        }
        handle = other.handle;
        layout = other.layout;
        cache = other.cache;
        setLayouts = std::move(other.setLayouts);
        reflection = other.reflection;
        device = other.device;
        other.handle = VK_NULL_HANDLE;
        other.layout = VK_NULL_HANDLE;
        other.cache = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
    }
    return *this;
}
