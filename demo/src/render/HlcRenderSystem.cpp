#include "HlcRenderSystem.hpp"

//TEMP replace this with #include <cth_engine/cth_engine.hpp> 
#include "vulkan/memory/descriptor/CthDescriptorPool.hpp"
#include "vulkan/memory/descriptor/descriptors/CthImageDescriptors.hpp"
#include "vulkan/pipeline/CthPipeline.hpp"
#include "vulkan/pipeline/layout/CthDescriptorSetLayout.hpp"
#include "vulkan/pipeline/layout/CthPipelineLayout.hpp"
#include "vulkan/pipeline/shader/CthShader.hpp"

#include <span>
#include <glm/glm.hpp>

#include "vulkan/memory/descriptor/CthDescriptorSet.hpp"


namespace cth {
struct UniformBuffer {
    glm::mat4 projectionView;
    explicit UniformBuffer(const glm::mat4& projection_view) : projectionView{projection_view} {}
};

RenderSystem::RenderSystem(Device* device, VkRenderPass render_pass, const VkSampleCountFlagBits msaa_samples) : device{device} {
    createShaders();

    createPipelineLayout();
    createPipeline(render_pass, msaa_samples);

    createDefaultTriangle();
}
RenderSystem::~RenderSystem() {}

void RenderSystem::createShaders() {

    const string vertexBinary = format("{}shader.vert.spv", SHADER_BINARY_DIR);
    const string fragmentBinary = format("{}shader.frag.spv", SHADER_BINARY_DIR);

#ifdef _FINAL
    vertShader = make_unique<Shader>(device, VK_SHADER_STAGE_VERTEX_BIT, vertexBinary);
    fragShader = make_unique<Shader>(device, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentBinary);
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
    DescriptorPool::Builder builder{};
    builder.addLayout(descriptorSetLayout.get(), 1);

    descriptorPool = make_unique<DescriptorPool>(device, builder);
}
void RenderSystem::createDescriptorSets() {
    textureDescriptor = make_unique<TextureDescriptor>()

    DescriptorSet::Builder builder{};
    //TEMP left off here complete the first texture descriptor and let it display
    builder.addDescriptor()

    descriptorSet = make_unique<DescriptorSet>();
}



array<Vertex, 3> defaultTriangle{
    Vertex{{-1.f, -0.5f, 0.2f}, {1, 0, 0}, {}},
    Vertex{{0, 1.f, 0.2f}, {0, 1, 0}, {}},
    Vertex{{1.f, -0.5f, 0.2f}, {0, 0, 1}, {}},
};

void RenderSystem::createDefaultTriangle() {
    defaultTriangleBuffer = make_unique<Buffer<Vertex>>(device, 3,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    defaultTriangleBuffer->stage(span<Vertex>{defaultTriangle.data(), 3});
}

void RenderSystem::render(FrameInfo& frame_info) const {
    pipeline->bind(frame_info.commandBuffer);
    const vector<VkBuffer> vertexBuffers{defaultTriangleBuffer->get()};
    const vector<VkDeviceSize> offsets(vertexBuffers.size());

    vkCmdBindVertexBuffers(frame_info.commandBuffer, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(), offsets.data());

    //TEMP replace this with model drawing
    vkCmdDraw(frame_info.commandBuffer, static_cast<uint32_t>(defaultTriangleBuffer->size()), 1, 0, 0);


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

}


//void RenderSystem::initSamplers() {
//	defaultTextureSampler = make_unique<HlcTextureSampler>(hlcDevice, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
//}

/*void RenderSystem::initDescriptorUtils() {
    //descriptorPool = HlcDescriptorPool::Builder(hlcDevice)
    //                 .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT)
    //                 .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
    //                 // .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Swapchain::MAX_FRAMES_IN_FLIGHT)
    //                 .build();

    //descriptorSetLayout = HlcDescriptorSetLayout::Builder(hlcDevice)
    //                      .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
    //                      //.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
    //                      .build();
    //descriptorSets.resize(descriptorPool->descriptorPoolInfo.maxSets);
}
void RenderSystem::initDescriptorSets() {
    //vector<VkDescriptorBufferInfo> bufferInfos;
    //vector<VkDescriptorImageInfo> imageInfos;

    //uint32_t descriptorSetIndex = 0;
    //initDescriptedBuffers(bufferInfos);
    ////initDescriptedTextures(descriptorSetIndex, imageInfos);

    //for(int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++) {
    //    HlcDescriptorWriter(*descriptorSetLayout, *descriptorPool)
    //        .writeBuffer(0, &bufferInfos[i])
    //        .build(descriptorSets[i]);
    //}
}

void RenderSystem::initDescriptedBuffers(vector<VkDescriptorBufferInfo>& buffer_infos) {
    for(int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++) {
        descriptedBuffers.push_back(make_unique<Buffer>(hlcDevice,
            sizeof(UniformBuffer),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            hlcDevice.physicalProperties.limits.minUniformBufferOffsetAlignment));

        descriptedBuffers.back()->map();
        buffer_infos.push_back(descriptedBuffers.back()->descriptorInfo());
    }
}
//void RenderSystem::initDescriptedTextures(uint32_t& descriptor_set_index, vector<VkDescriptorImageInfo>& image_infos) {
//	image_infos.resize(IMAGES);
//	descriptedImages.resize(IMAGES);
//
//	descriptedImages[WALL] = make_unique<Image>("resources/textures/wall.png", hlcDevice);
//	image_infos[WALL] = descriptedImages[WALL]->getDescriptorInfo(defaultTextureSampler->sampler());
//}
*/