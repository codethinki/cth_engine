#include "HlcRenderSystem.hpp"

//TEMP replace this with #include <cth_engine/cth_engine.hpp> 
#include "vulkan/pipeline/CthPipeline.hpp"
#include "vulkan/pipeline/layout/CthDescriptorSetLayout.hpp"
#include "vulkan/pipeline/layout/CthPipelineLayout.hpp"
#include "vulkan/pipeline/shader/CthShader.hpp"
#include "vulkan/resource/descriptor/CthDescriptorPool.hpp"
#include "vulkan/resource/descriptor/descriptors/CthImageDescriptors.hpp"

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/descriptor/CthDescriptorSet.hpp"
#include "vulkan/resource/image/texture/CthTexture.hpp"

#include <cth/cth_image.h>

#include <span>
#include <glm/glm.hpp>

#include "interface/render/CthRenderer.hpp"


namespace cth {
struct UniformBuffer {
    glm::mat4 projectionView;
    explicit UniformBuffer(const glm::mat4& projection_view) : projectionView{projection_view} {}
};
//TEMP renderer should not be here
RenderSystem::RenderSystem(Device* device, Renderer* renderer, VkRenderPass render_pass, const VkSampleCountFlagBits msaa_samples) : device{device},
    renderer(renderer) {
    auto initCmdBuffer = renderer->beginInitCmdBuffer();
    createShaders();

    createDescriptorSetLayouts();

    createPipelineLayout();

    createDescriptorPool();
    loadDescriptorData(initCmdBuffer, renderer->deletionQueue());

    createPipeline(render_pass, msaa_samples);

    createDescriptorSets();

    createDefaultTriangle(initCmdBuffer);

    renderer->endInitBuffer();
}
RenderSystem::~RenderSystem() {}

void RenderSystem::createShaders() {

    const string vertexBinary = format("{}shader.vert.spv", SHADER_BINARY_DIR);
    const string fragmentBinary = format("{}shader.frag.spv", SHADER_BINARY_DIR);

#ifdef _FINAL
    vertexShader = make_unique<Shader>(device, VK_SHADER_STAGE_VERTEX_BIT, vertexBinary.data());
    fragmentShader = make_unique<Shader>(device, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentBinary.data());
#else
    vertexShader = make_unique<Shader>(device, VK_SHADER_STAGE_VERTEX_BIT, vertexBinary,
        format("{}shader.vert", SHADER_GLSL_DIR), GLSL_COMPILER_PATH);
    fragmentShader = make_unique<Shader>(device, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentBinary,
        format("{}shader.frag", SHADER_GLSL_DIR), GLSL_COMPILER_PATH);
#endif
}
void RenderSystem::createDescriptorSetLayouts() {
    DescriptorSetLayout::Builder builder{};
    builder.addBinding(0, TextureDescriptor::TYPE, VK_SHADER_STAGE_FRAGMENT_BIT, 1);


    descriptorSetLayout = make_unique<DescriptorSetLayout>(device, builder);
}


void RenderSystem::createPipelineLayout() {
    PipelineLayout::Builder builder{};
    builder.addSetLayout(descriptorSetLayout.get(), 0);

    pipelineLayout = make_unique<PipelineLayout>(device, builder);
}
void RenderSystem::createPipeline(const VkRenderPass render_pass, const VkSampleCountFlagBits msaa_samples) {
    Pipeline::GraphicsConfig config = Pipeline::GraphicsConfig::createDefault();

    config.renderPass = render_pass;
    config.multisampleInfo->rasterizationSamples = msaa_samples;
    config.addShaderStage(vertexShader.get());
    config.addShaderStage(fragmentShader.get());

    pipeline = std::make_unique<Pipeline>(device, pipelineLayout.get(), config);
}
void RenderSystem::createDescriptorPool() {
    descriptorPool = make_unique<DescriptorPool>(device, DescriptorPool::Builder{{{descriptorSetLayout.get(), 1}}});
}
void RenderSystem::loadDescriptorData(const CmdBuffer* init_cmd_buffer, DeletionQueue* deletion_queue) {
    const cth::img::stb_image image{std::format("{}first_texture.png", TEXTURE_DIR), 4};

    texture = make_unique<Texture>(device, deletion_queue, VkExtent2D{image.width, image.height}, Texture::Config{VK_FORMAT_R8G8B8A8_SRGB}, init_cmd_buffer, image.raw());
}


void RenderSystem::createDescriptorSets() {
    textureSampler = make_unique<Sampler>(device, Sampler::Config::Default());


    textureView = make_unique<ImageView>(device, texture.get(), ImageView::Config::Default());
    textureDescriptor = make_unique<TextureDescriptor>(textureView.get(), textureSampler.get());

    //TEMP left off here complete the first texture descriptor and let it display

    descriptorSet = make_unique<DescriptorSet>(DescriptorSet::Builder{descriptorSetLayout.get(), vector<Descriptor*>{textureDescriptor.get()}});

    descriptorPool->writeSets(vector{descriptorSet.get()});
}
array<Vertex, 3> defaultTriangle{
    Vertex{{1.f, -0.5f, 0.2f}, {0, 0, 1}, {1, 0}},
    Vertex{{0, 1.f, 0.2f}, {0, 1, 0}, {0.5, 1}},
    Vertex{{-1.f, -0.5f, 0.2f}, {1, 0, 0}, {0, 0}},
};



void RenderSystem::createDefaultTriangle(const CmdBuffer* cmd_buffer) {
    defaultTriangleBuffer = make_unique<Buffer<Vertex>>(device, renderer->deletionQueue(), 3,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Buffer<Vertex> stagingBuffer{device, renderer->deletionQueue(), 3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT};
    stagingBuffer.map();
    stagingBuffer.write(defaultTriangle);

    defaultTriangleBuffer->stage(cmd_buffer, &stagingBuffer);
}

void RenderSystem::render(FrameInfo& frame_info) const {
    pipeline->bind(frame_info.commandBuffer);
    const vector<VkBuffer> vertexBuffers{defaultTriangleBuffer->get()};
    const vector<size_t> offsets(vertexBuffers.size());
    const vector<VkDescriptorSet> descriptorSets{descriptorSet->get()};

    vkCmdBindDescriptorSets(frame_info.commandBuffer->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout->get(), 0, 1, descriptorSets.data(), 0,
        nullptr);
    vkCmdBindVertexBuffers(frame_info.commandBuffer->get(), 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), offsets.data());

    //TEMP replace this with model drawing
    vkCmdDraw(frame_info.commandBuffer->get(), static_cast<uint32_t>(defaultTriangleBuffer->size()), 1, 0, 0);


    //const UniformBuffer uniformBuffer{frame_info.camera.getProjection() * frame_info.camera.getView()};
    //descriptedBuffers[frame_info.frameIndex]->writeToBuffer(&uniformBuffer);
    //descriptedBuffers[frame_info.frameIndex]->flush();


    /*vkCmdBindDescriptorSets(frame_info.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
        &descriptorSets[frame_info.frameIndex], 0, nullptr);*/

    frame_info.pipelineLayout = pipelineLayout->get();


    /*uint32_t index = 0;
    for(int i = 0; i < render_data.groupSizes.size(); i++) {
        RenderObject::Render_Group group = RenderObject::RENDER_GROUP_INVALID;
        switch(i) {
            case 0:
                group = RenderObject::RENDER_GROUP_STATIC;
                break;
            case 1:
                group = RenderObject::RENDER_GROUP_STATIC_VERTICES;
                break;
            case 2:
                group = RenderObject::RENDER_GROUP_STATIC_INDICES;
                break;
            case 3:
                group = RenderObject::RENDER_GROUP_DYNAMIC;
                break;
        }

        assert(group != RenderObject::RENDER_GROUP_INVALID && "render: invalid render group");

        if(memoryManager.allowedRenderGroups & group) memoryManager.bind(group, frame_info);

        for(int k = 0; k < render_data.groupSizes[i]; k++)
            render_data.objects[render_data.groupIndices[index + k]]->render(
                group, frame_info);

        index += render_data.groupSizes[i];
    }*/
}

} // namespace cth

/*

void RenderSystem::initDescriptedBuffers(vector<VkDescriptorBufferInfo>& buffer_infos) {
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        descriptedBuffers.push_back(make_unique<Buffer>(hlcDevice,
            sizeof(UniformBuffer),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            hlcDevice.physicalProperties.limits.minUniformBufferOffsetAlignment));

        descriptedBuffers.back()->map();
        buffer_infos.push_back(descriptedBuffers.back()->descriptorInfo());
    }
} */
