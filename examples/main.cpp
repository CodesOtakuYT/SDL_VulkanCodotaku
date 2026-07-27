#include "App.h"
#include "Pipeline.h"
#include "Sync.h"
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

// 36 vertices, non-indexed
static Vertex cubeVertices[] = {
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.5f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.5f, 0.5f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.5f, 0.0f, 0.5f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {0.5f, 0.0f, 0.5f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.5f, 0.5f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.5f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.5f, 0.5f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.5f, 1.0f}},
};

// 4 vertices, indexed with 6 indices — tests index buffer upload
static Vertex quadVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f, 0.0f}, {0.5f, 1.0f, 0.5f}},
};

static uint32_t quadIndices[] = {
    0, 1, 2,
    2, 3, 0,
};

// 3 vertices, non-indexed — odd size (36 bytes) to stress alignment
static Vertex triangleVertices[] = {
    {{ 0.0f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
};

class CubeApp : public App {
    Pipeline pipeline;
    uint32_t depthHandle = 0;
    uint32_t msaaColorHandle = 0;
    uint32_t msaaDepthHandle = 0;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    float angle = 0.0f;

    struct Mesh {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VmaAllocation vertexAllocation = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VmaAllocation indexAllocation = VK_NULL_HANDLE;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
    };

    Mesh cube;
    Mesh quad;
    Mesh triangle;

    Mesh createMesh(const Vertex *verts, uint32_t vertCount,
                    const uint32_t *inds, uint32_t indCount) {
        Mesh m;
        m.vertexCount = vertCount;
        m.indexCount = indCount;

        {
            VkBufferCreateInfo bufInfo{};
            bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufInfo.size = vertCount * sizeof(Vertex);
            bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            VmaAllocationCreateInfo vmaInfo{};
            vmaInfo.usage = VMA_MEMORY_USAGE_AUTO;
            vmaInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            alloc.createBuffer(bufInfo, vmaInfo, &m.vertexBuffer, &m.vertexAllocation);
        }

        if (inds && indCount > 0) {
            VkBufferCreateInfo bufInfo{};
            bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufInfo.size = indCount * sizeof(uint32_t);
            bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            VmaAllocationCreateInfo vmaInfo{};
            vmaInfo.usage = VMA_MEMORY_USAGE_AUTO;
            vmaInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            alloc.createBuffer(bufInfo, vmaInfo, &m.indexBuffer, &m.indexAllocation);
        }

        return m;
    }

    void destroyMesh(Mesh &m) {
        if (m.indexBuffer != VK_NULL_HANDLE) {
            alloc.destroyBuffer(m.indexBuffer, m.indexAllocation);
            m.indexBuffer = VK_NULL_HANDLE;
        }
        if (m.vertexBuffer != VK_NULL_HANDLE) {
            alloc.destroyBuffer(m.vertexBuffer, m.vertexAllocation);
            m.vertexBuffer = VK_NULL_HANDLE;
        }
    }

    void init() override {
        msaaSamples = ctx.maxSampleCount();
        printf("MSAA: %dx\n", msaaSamples);

        auto vert = compileShader(vertexShaderGLSL, VK_SHADER_STAGE_VERTEX_BIT, "cube.vert");
        auto frag = compileShader(fragmentShaderGLSL, VK_SHADER_STAGE_FRAGMENT_BIT, "cube.frag");

        cube = createMesh(cubeVertices, 36, nullptr, 0);
        quad = createMesh(quadVertices, 4, quadIndices, 6);
        triangle = createMesh(triangleVertices, 3, nullptr, 0);

        uploader.add(cubeVertices, sizeof(cubeVertices), cube.vertexBuffer, 0);
        uploader.add(quadVertices, sizeof(quadVertices), quad.vertexBuffer, 0);
        uploader.add(quadIndices, sizeof(quadIndices), quad.indexBuffer, 0);
        uploader.add(triangleVertices, sizeof(triangleVertices), triangle.vertexBuffer, 0);
        uploader.upload();

        printf("Uploaded: cube VB=%zu, quad VB=%zu IB=%zu, tri VB=%zu\n",
               sizeof(cubeVertices), sizeof(quadVertices), sizeof(quadIndices), sizeof(triangleVertices));

        msaaColorHandle = gbuffer.add({"MSAA Color", swap.imageFormat, msaaSamples,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT});

        msaaDepthHandle = gbuffer.add({"MSAA Depth", VK_FORMAT_D32_SFLOAT, msaaSamples,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT});

        depthHandle = gbuffer.add({"Depth", VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
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

    void drawMesh(VkCommandBuffer cmd, const Mesh &m, const glm::mat4 &mvp) {
        vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &m.vertexBuffer, &offset);

        if (m.indexCount > 0) {
            vkCmdBindIndexBuffer(cmd, m.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, m.indexCount, 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, m.vertexCount, 1, 0, 0);
        }
    }

    void recordFrame(const FrameInfo &frame) override {
        VkCommandBuffer cmd = frame.commandBuffer;
        bool useMsaa = msaaSamples > VK_SAMPLE_COUNT_1_BIT;

        sync::discardToGeneral(cmd, swap.images[frame.imageIndex]);
        if (useMsaa) {
            sync::discardToGeneral(cmd, gbuffer.getImage(msaaColorHandle));
            sync::discardDepthToGeneral(cmd, gbuffer.getImage(msaaDepthHandle));
        }
        sync::discardDepthToGeneral(cmd, gbuffer.getImage(depthHandle));

        VkClearValue colorClear = {{{0.01f, 0.01f, 0.033f, 1.0f}}};
        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = colorClear;

        if (useMsaa) {
            colorAttachment.imageView = gbuffer.getView(msaaColorHandle);
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
            depthAttachment.imageView = gbuffer.getView(msaaDepthHandle);
        } else {
            depthAttachment.imageView = gbuffer.getView(depthHandle);
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
        glm::mat4 view = glm::lookAt(glm::vec3(3.0f, 2.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        proj[1][1] *= -1;

        // Cube: rotating at origin
        glm::mat4 cubeModel = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1.0f, 1.0f, 0.0f));
        drawMesh(cmd, cube, proj * view * cubeModel);

        // Quad: stationary, offset to the right
        glm::mat4 quadModel = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
        drawMesh(cmd, quad, proj * view * quadModel);

        // Triangle: rotating opposite direction, offset to the left
        glm::mat4 triModel = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
        triModel = glm::rotate(triModel, -angle * 0.7f, glm::vec3(0.0f, 1.0f, 0.0f));
        drawMesh(cmd, triangle, proj * view * triModel);

        vkCmdEndRendering(cmd);
        sync::toPresent(cmd, swap.images[frame.imageIndex]);
    }

    void cleanup() override {
        vkDeviceWaitIdle(ctx.device);

        pipeline.destroy(ctx.device);
        destroyMesh(cube);
        destroyMesh(quad);
        destroyMesh(triangle);
    }

    void update(float dt) override {
        angle += dt * 1.5f;
    }
};

int main(int argc, char *argv[]) {
    try {
        CubeApp app;
        app.run("Multi-Mesh Upload Test", {1280, 720});
    } catch (const VkbError &e) {
        fprintf(stderr, "FATAL: %s\n", e.what());
        return 1;
    }
    return 0;
}
