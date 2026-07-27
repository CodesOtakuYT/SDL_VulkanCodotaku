#include "App.h"
#include "Shader.h"
#include "Sync.h"
#include <cstdio>
#include <glm/glm.hpp>

struct Vertex {
  glm::vec2 position;
  glm::vec3 color;
};

static const char *vertexShaderGLSL = R"(
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
)";

static const char *fragmentShaderGLSL = R"(
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
)";

class TriangleApp : public App {
  ShaderCompiler compiler;
  ShaderModule vertModule;
  ShaderModule fragModule;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  VkBuffer vertexBuffer = VK_NULL_HANDLE;
  VmaAllocation vertexAllocation = VK_NULL_HANDLE;

  void init() override {
    auto &ctx = getContext();
    auto &alloc = getAllocator();

    // Init shader compiler
    if (!compiler.init()) {
      fprintf(stderr, "Failed to init shaderc\n");
      return;
    }

    // Compile shaders from inline GLSL
    if (!vertModule.createFromGLSL(ctx.device, compiler, vertexShaderGLSL,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   "triangle.vert")) {
      fprintf(stderr, "Failed to compile vertex shader\n");
      return;
    }

    if (!fragModule.createFromGLSL(ctx.device, compiler, fragmentShaderGLSL,
                                   VK_SHADER_STAGE_FRAGMENT_BIT,
                                   "triangle.frag")) {
      fprintf(stderr, "Failed to compile fragment shader\n");
      return;
    }

    // Create vertex buffer via VMA
    Vertex vertices[] = {
        {{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    };

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sizeof(vertices);
    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vmaInfo{};
    vmaInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vmaInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    alloc.createBuffer(bufInfo, vmaInfo, &vertexBuffer, &vertexAllocation);

    void *data;
    alloc.mapMemory(vertexAllocation, &data);
    memcpy(data, vertices, sizeof(vertices));
    alloc.unmapMemory(vertexAllocation);

    // Pipeline layout (empty)
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkCreatePipelineLayout(ctx.device, &layoutInfo, nullptr, &pipelineLayout);

    // Pipeline
    createPipeline();
  }

  void createPipeline() {
    auto &ctx = getContext();
    auto &swap = getSwapchain();

    VkPipelineShaderStageCreateInfo stages[] = {vertModule.stageCreateInfo(),
                                                fragModule.stageCreateInfo()};

    // Vertex input
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[2]{};
    attrDescs[0].location = 0;
    attrDescs[0].binding = 0;
    attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, position);

    attrDescs[1].location = 1;
    attrDescs[1].binding = 0;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attrDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Dynamic rendering - no render pass needed
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &swap.imageFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipelineInfo,
                              nullptr, &pipeline);
  }

  void recordFrame(const FrameInfo &frame) override {
    VkCommandBuffer cmd = frame.commandBuffer;

    // Discard previous contents, transition to GENERAL
    sync::discardToGeneral(cmd, getSwapchain().images[frame.imageIndex]);

    // Begin rendering
    VkClearValue clearValue = {{{0.01f, 0.01f, 0.033f, 1.0f}}};
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = frame.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clearValue;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset = {0, 0};
    renderInfo.renderArea.extent = frame.extent;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderInfo);

    // Bind pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Dynamic viewport
    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(frame.extent.width);
    vp.height = static_cast<float>(frame.extent.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    // Dynamic scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = frame.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind vertex buffer & draw
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);

    // Transition to present
    sync::toPresent(cmd, getSwapchain().images[frame.imageIndex]);
  }

  void cleanup() override {
    auto &ctx = getContext();
    vkDeviceWaitIdle(ctx.device);

    vertModule.destroy(ctx.device);
    fragModule.destroy(ctx.device);

    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(ctx.device, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
      pipelineLayout = VK_NULL_HANDLE;
    }
    if (vertexBuffer != VK_NULL_HANDLE) {
      getAllocator().destroyBuffer(vertexBuffer, vertexAllocation);
      vertexBuffer = VK_NULL_HANDLE;
      vertexAllocation = VK_NULL_HANDLE;
    }

    compiler.shutdown();
  }

  void resize(uint32_t w, uint32_t h) override {
    auto &ctx = getContext();
    vkDeviceWaitIdle(ctx.device);

    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(ctx.device, pipeline, nullptr);
      pipeline = VK_NULL_HANDLE;
    }

    createPipeline();
  }
};

int main(int argc, char *argv[]) {
  TriangleApp app;
  app.run("Vulkan Triangle", 1280, 720);
  return 0;
}
