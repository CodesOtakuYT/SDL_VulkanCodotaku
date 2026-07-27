#include "App.h"
#include "Pipeline.h"
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
    Pipeline pipeline;
    ShaderCompiler compiler;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;

    void init() override {
        auto &ctx = getContext();
        auto &swap = getSwapchain();
        auto &alloc = getAllocator();

        compiler.init();

        // Compile shaders
        ShaderModule vert, frag;
        if (!vert.createFromGLSL(ctx.device, compiler, vertexShaderGLSL,
                                 VK_SHADER_STAGE_VERTEX_BIT, "triangle.vert")) {
            fprintf(stderr, "Failed to compile vertex shader\n");
            return;
        }
        if (!frag.createFromGLSL(ctx.device, compiler, fragmentShaderGLSL,
                                 VK_SHADER_STAGE_FRAGMENT_BIT, "triangle.frag")) {
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

        // Create pipeline — layout auto-generated from reflection
        pipeline = Pipeline::create(ctx.device, {
            .shaders = {
                {vert.spirv, VK_SHADER_STAGE_VERTEX_BIT, "triangle.vert"},
                {frag.spirv, VK_SHADER_STAGE_FRAGMENT_BIT, "triangle.frag"},
            },
            .colorAttachmentFormat = swap.imageFormat,
        });

        vert.destroy(ctx.device);
        frag.destroy(ctx.device);
    }

    void recordFrame(const FrameInfo &frame) override {
        VkCommandBuffer cmd = frame.commandBuffer;

        sync::discardToGeneral(cmd, getSwapchain().images[frame.imageIndex]);

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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

        VkViewport vp{};
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = static_cast<float>(frame.extent.width);
        vp.height = static_cast<float>(frame.extent.height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = frame.extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);

        sync::toPresent(cmd, getSwapchain().images[frame.imageIndex]);
    }

    void cleanup() override {
        auto &ctx = getContext();
        vkDeviceWaitIdle(ctx.device);

        pipeline.destroy(ctx.device);

        if (vertexBuffer != VK_NULL_HANDLE) {
            getAllocator().destroyBuffer(vertexBuffer, vertexAllocation);
            vertexBuffer = VK_NULL_HANDLE;
            vertexAllocation = VK_NULL_HANDLE;
        }

        compiler.shutdown();
    }

    void resize(uint32_t w, uint32_t h) override {
        auto &ctx = getContext();
        auto &swap = getSwapchain();
        vkDeviceWaitIdle(ctx.device);

        pipeline.destroy(ctx.device);

        // Recompile shaders for reflection
        ShaderModule vert, frag;
        vert.createFromGLSL(ctx.device, compiler, vertexShaderGLSL,
                            VK_SHADER_STAGE_VERTEX_BIT, "triangle.vert");
        frag.createFromGLSL(ctx.device, compiler, fragmentShaderGLSL,
                            VK_SHADER_STAGE_FRAGMENT_BIT, "triangle.frag");

        pipeline = Pipeline::create(ctx.device, {
            .shaders = {
                {vert.spirv, VK_SHADER_STAGE_VERTEX_BIT, "triangle.vert"},
                {frag.spirv, VK_SHADER_STAGE_FRAGMENT_BIT, "triangle.frag"},
            },
            .colorAttachmentFormat = swap.imageFormat,
        });

        vert.destroy(ctx.device);
        frag.destroy(ctx.device);
    }
};

int main(int argc, char *argv[]) {
    TriangleApp app;
    app.run("Vulkan Triangle", 1280, 720);
    return 0;
}
