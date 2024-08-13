#include "HlcRenderSystem.hpp"

#include "interface/render/CthRenderer.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/pipeline/CthPipeline.hpp"
#include "vulkan/render/pipeline/layout/CthDescriptorSetLayout.hpp"
#include "vulkan/render/pipeline/layout/CthPipelineLayout.hpp"
#include "vulkan/render/pipeline/shader/CthShader.hpp"
#include "vulkan/resource/descriptor/CthDescriptorPool.hpp"
#include "vulkan/resource/descriptor/CthDescriptorSet.hpp"
#include "vulkan/resource/descriptor/descriptors/CthImageDescriptors.hpp"
#include "vulkan/resource/image/texture/CthTexture.hpp"

#include <cth/cth_image.hpp>


namespace cth {
struct UniformBuffer {
    glm::mat4 projectionView;
    explicit UniformBuffer(glm::mat4 const& projection_view) : projectionView{projection_view} {}
};
//TEMP renderer should not be here
RenderSystem::RenderSystem(vk::BasicCore const* core, vk::DestructionQueue* destruction_queue, vk::PrimaryCmdBuffer const& init_cmd_buffer,
    VkRenderPass render_pass, VkSampleCountFlagBits const msaa_samples) : _core
    {core} {
    createShaders();

    createDescriptorSetLayouts();

    createPipelineLayout();

    createDescriptorPool();
    loadDescriptorData(init_cmd_buffer, destruction_queue);

    createPipeline(render_pass, msaa_samples);

    createDescriptorSets();

    createDefaultTriangle(init_cmd_buffer, destruction_queue);

}

void RenderSystem::createShaders() {

    std::string const vertexBinary = std::format("{}shader.vert.spv", SHADER_BINARY_DIR);
    std::string const fragmentBinary = std::format("{}shader.frag.spv", SHADER_BINARY_DIR);

#ifndef CONSTANT_DEBUG_MODE
    vertexShader = make_unique<Shader>(_device, VK_SHADER_STAGE_VERTEX_BIT, vertexBinary.data());
    fragmentShader = make_unique<Shader>(_device, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentBinary.data());
#else
    _vertexShader = std::make_unique<vk::Shader>(_core, VK_SHADER_STAGE_VERTEX_BIT, vertexBinary,
        std::format("{}shader.vert", SHADER_GLSL_DIR), GLSL_COMPILER_PATH);
    _fragmentShader = std::make_unique<vk::Shader>(_core, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentBinary,
        std::format("{}shader.frag", SHADER_GLSL_DIR), GLSL_COMPILER_PATH);
#endif
}
void RenderSystem::createDescriptorSetLayouts() {
    vk::DescriptorSetLayout::Builder builder{};
    builder.addBinding(0, vk::TextureDescriptor::TYPE, VK_SHADER_STAGE_FRAGMENT_BIT, 1);


    _descriptorSetLayout = std::make_unique<vk::DescriptorSetLayout>(_core, builder);
}


void RenderSystem::createPipelineLayout() {
    vk::PipelineLayout::Builder builder{};
    builder.addSetLayout(_descriptorSetLayout.get(), 0);

    _pipelineLayout = std::make_unique<vk::PipelineLayout>(_core, builder);
}
void RenderSystem::createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits const msaa_samples) {
    vk::Pipeline::GraphicsConfig config = vk::Pipeline::GraphicsConfig::createDefault();

    config.renderPass = render_pass;
    config.multisampleInfo->rasterizationSamples = msaa_samples;
    config.addShaderStage(_vertexShader.get());
    config.addShaderStage(_fragmentShader.get());


    _pipeline = std::make_unique<vk::Pipeline>(_core, _pipelineLayout.get(), config);
}
void RenderSystem::createDescriptorPool() {
    _descriptorPool = std::make_unique<vk::DescriptorPool>(_core, vk::DescriptorPool::Builder{{{_descriptorSetLayout.get(), 1}}});
}
void RenderSystem::loadDescriptorData(vk::CmdBuffer const& init_cmd_buffer, vk::DestructionQueue* destruction_queue) {
    cth::img::stb_image const image{std::format("{}first_texture.png", TEXTURE_DIR), 4};

    _texture = std::make_unique<vk::Texture>(_core, destruction_queue, VkExtent2D{image.width(), image.height()},
        vk::Texture::Config{VK_FORMAT_R8G8B8A8_SRGB},
        init_cmd_buffer, image.raw());
}


void RenderSystem::createDescriptorSets() {
    _textureSampler = std::make_unique<vk::Sampler>(_core, vk::Sampler::Config::Default());


    _textureView = std::make_unique<vk::ImageView>(_core, _texture.get(), vk::ImageView::Config::Default());
    _textureDescriptor = std::make_unique<vk::TextureDescriptor>(_textureView.get(), _textureSampler.get());

    _descriptorSet = std::make_unique<vk::DescriptorSet>(
        vk::DescriptorSet::Builder{_descriptorSetLayout.get(), std::vector<vk::Descriptor*>{_textureDescriptor.get()}});

    _descriptorPool->writeSets(std::vector{_descriptorSet.get()});
}
std::array<vk::Vertex, 3> defaultTriangle{
    vk::Vertex{{1.f, -0.5f, 0.2f}, {0, 0, 1}, {1, 0}},
    vk::Vertex{{0, 1.f, 0.2f}, {0, 1, 0}, {0.5, 1}},
    vk::Vertex{{-1.f, -0.5f, 0.2f}, {1, 0, 0}, {0, 0}},
};



void RenderSystem::createDefaultTriangle(vk::CmdBuffer const& cmd_buffer, vk::DestructionQueue* destruction_queue) {
    _defaultTriangleBuffer = std::make_unique<vk::Buffer<vk::Vertex>>(_core, destruction_queue, 3,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk::Buffer<vk::Vertex> stagingBuffer{_core, destruction_queue, 3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT};
    stagingBuffer.map();
    stagingBuffer.write(defaultTriangle);

    _defaultTriangleBuffer->stage(cmd_buffer, stagingBuffer);
}

void RenderSystem::render(FrameInfo& frame_info) const {
    _pipeline->bind(frame_info.commandBuffer);
    std::vector<VkBuffer> const vertexBuffers{_defaultTriangleBuffer->get()};
    std::vector<size_t> const offsets(vertexBuffers.size());
    std::vector<VkDescriptorSet> const descriptorSets{_descriptorSet->get()};

    vkCmdBindDescriptorSets(frame_info.commandBuffer->get(), VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout->get(), 0, 1, descriptorSets.data(), 0,
        nullptr);
    vkCmdBindVertexBuffers(frame_info.commandBuffer->get(), 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), offsets.data());

    //TEMP replace this with model drawing
    vkCmdDraw(frame_info.commandBuffer->get(), static_cast<uint32_t>(_defaultTriangleBuffer->size()), 1, 0, 0);


    //const UniformBuffer uniformBuffer{frame_info.camera.getProjection() * frame_info.camera.getView()};
    //descriptedBuffers[frame_info.frameIndex]->writeToBuffer(&uniformBuffer);
    //descriptedBuffers[frame_info.frameIndex]->flush();


    /*vkCmdBindDescriptorSets(frame_info.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
        &descriptorSets[frame_info.frameIndex], 0, nullptr);*/

    frame_info.pipelineLayout = _pipelineLayout->get();


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
