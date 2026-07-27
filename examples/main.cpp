#include "App.h"
#include "Pipeline.h"
#include "Sync.h"
#include "Uploader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

static const char *vertexShaderGLSL = R"(
#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
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

static Vertex cubeVertices[] = {
    // Front face
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}},
    // Back face
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
    // Top face
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.5f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.5f, 0.5f}},
    // Bottom face
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.5f, 0.0f, 0.5f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.5f, 0.0f, 0.5f}},
    // Right face
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.5f, 0.5f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.5f, 1.0f}},
    // Left face
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.5f, 0.5f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.5f, 1.0f}},
};

static VkSampleCountFlagBits maxSampleCount(VkPhysicalDeviceProperties &props) {
    VkSampleCountFlags color = props.limits.framebufferColorSampleCounts;
    VkSampleCountFlags depth = props.limits.framebufferDepthSampleCounts;
    VkSampleCountFlags both = color & depth;

    if (both & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
    if (both & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
    if (both & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

class CubeApp : public App {
    Pipeline pipeline;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAllocation = VK_NULL_HANDLE;
    uint32_t depthHandle = 0;
    uint32_t msaaColorHandle = 0;
    uint32_t msaaDepthHandle = 0;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    Uploader uploader;
    float angle = 0.0f;

    void init() override {
        auto &ctx = getContext();
        auto &swap = getSwapchain();
        auto &alloc = getAllocator();

        msaaSamples = maxSampleCount(ctx.deviceProperties);
        printf("MSAA: %dx\n", msaaSamples);

        auto vert = compileShader(vertexShaderGLSL, VK_SHADER_STAGE_VERTEX_BIT, "cube.vert");
        auto frag = compileShader(fragmentShaderGLSL, VK_SHADER_STAGE_FRAGMENT_BIT, "cube.frag");

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = sizeof(cubeVertices);
        bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo vmaInfo{};
        vmaInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        alloc.createBuffer(bufInfo, vmaInfo, &vertexBuffer, &vertexAllocation);

        uploader.init(ctx, alloc);
        uploader.add(cubeVertices, sizeof(cubeVertices), vertexBuffer, 0);
        uploader.upload();

        msaaColorHandle = getGBuffer().add({"MSAA Color", swap.imageFormat, msaaSamples,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT});

        msaaDepthHandle = getGBuffer().add({"MSAA Depth", VK_FORMAT_D32_SFLOAT, msaaSamples,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT});

        depthHandle = getGBuffer().add({"Depth", VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});

        pipeline = Pipeline::create(ctx.device, {
            .shaders = {
                {vert.spirv, VK_SHADER_STAGE_VERTEX_BIT, "cube.vert"},
                {frag.spirv, VK_SHADER_STAGE_FRAGMENT_BIT, "cube.frag"},
            },
            .colorAttachmentFormat = swap.imageFormat,
            .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
            .samples = msaaSamples,
        });

        vert.destroy(ctx.device);
        frag.destroy(ctx.device);
    }

    void recordFrame(const FrameInfo &frame) override {
        VkCommandBuffer cmd = frame.commandBuffer;
        auto &swap = getSwapchain();
        bool useMsaa = msaaSamples > VK_SAMPLE_COUNT_1_BIT;

        sync::discardToGeneral(cmd, swap.images[frame.imageIndex]);
        if (useMsaa) {
            sync::discardToGeneral(cmd, getGBuffer().getImage(msaaColorHandle));
            sync::discardDepthToGeneral(cmd, getGBuffer().getImage(msaaDepthHandle));
        }
        sync::discardDepthToGeneral(cmd, getGBuffer().getImage(depthHandle));

        VkClearValue colorClear = {{{0.01f, 0.01f, 0.033f, 1.0f}}};
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = colorClear;

        if (useMsaa) {
            colorAttachment.imageView = getGBuffer().getView(msaaColorHandle);
            colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
            colorAttachment.resolveImageView = frame.imageView;
            colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        } else {
            colorAttachment.imageView = frame.imageView;
        }

        VkClearValue depthClear = {{{1.0f, 0.0f}}};
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue = depthClear;

        if (useMsaa) {
            depthAttachment.imageView = getGBuffer().getView(msaaDepthHandle);
        } else {
            depthAttachment.imageView = getGBuffer().getView(depthHandle);
        }

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.renderArea.offset = {0, 0};
        renderInfo.renderArea.extent = {frame.extent.x, frame.extent.y};
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;
        renderInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);

        VkViewport vp{};
        vp.x = 0.0f;
        vp.y = 0.0f;
        vp.width = static_cast<float>(frame.extent.x);
        vp.height = static_cast<float>(frame.extent.y);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {frame.extent.x, frame.extent.y};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        float aspect = static_cast<float>(frame.extent.x) / static_cast<float>(frame.extent.y);
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 1.0f, 0.0f));
        glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
        proj[1][1] *= -1;
        glm::mat4 mvp = proj * view * model;

        vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdDraw(cmd, 36, 1, 0, 0);

        vkCmdEndRendering(cmd);

        sync::toPresent(cmd, swap.images[frame.imageIndex]);
    }

    void cleanup() override {
        auto &ctx = getContext();
        vkDeviceWaitIdle(ctx.device);

        pipeline.destroy(ctx.device);
        uploader.destroy();

        if (vertexBuffer != VK_NULL_HANDLE) {
            getAllocator().destroyBuffer(vertexBuffer, vertexAllocation);
        }
    }

    void update(float dt) override {
        angle += dt * 1.5f;
    }
};

int main(int argc, char *argv[]) {
    try {
        CubeApp app;
        app.run("Vulkan Cube", {1280, 720});
    } catch (const VkbError &e) {
        fprintf(stderr, "FATAL: %s\n", e.what());
        return 1;
    }
    return 0;
}
