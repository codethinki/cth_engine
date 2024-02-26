import hlc.math;

#include "HlcRenderSystem.hpp"

#include "HlcPushConstant.hpp"
#include "../core/HlcSwapchain.hpp"
#include "../memory/HlcMemoryManager.hpp"
#include "../objects/HlcRenderObject.hpp"

#include <stdexcept>
#include <glm/glm.hpp>




namespace cth {
struct UniformBuffer {
    glm::mat4 projectionView;
    explicit UniformBuffer(const glm::mat4& projection_view) : projectionView{projection_view} {}
};

RenderSystem::RenderSystem(Device& device, MemoryManager& memory_manager, VkRenderPass render_pass, const VkSampleCountFlagBits msaa_samples) : hlcDevice{device},
    memoryManager{memory_manager} {
    //initSamplers();
    initDescriptorUtils();

    initDescriptorSets();

    createPipelineLayout();
    createPipeline(render_pass, msaa_samples);
}
RenderSystem::~RenderSystem() { vkDestroyPipelineLayout(hlcDevice.device(), pipelineLayout, nullptr); }

//void RenderSystem::initSamplers() {
//	defaultTextureSampler = make_unique<HlcTextureSampler>(hlcDevice, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
//}

void RenderSystem::initDescriptorUtils() {
    descriptorPool = HlcDescriptorPool::Builder(hlcDevice)
                     .setMaxSets(HlcSwapchain::MAX_FRAMES_IN_FLIGHT)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, HlcSwapchain::MAX_FRAMES_IN_FLIGHT)
                     // .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, HlcSwapchain::MAX_FRAMES_IN_FLIGHT)
                     .build();

    descriptorSetLayout = HlcDescriptorSetLayout::Builder(hlcDevice)
                          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                          //.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                          .build();
    descriptorSets.resize(descriptorPool->descriptorPoolInfo.maxSets);
}
void RenderSystem::initDescriptorSets() {
    vector<VkDescriptorBufferInfo> bufferInfos;
    vector<VkDescriptorImageInfo> imageInfos;

    uint32_t descriptorSetIndex = 0;
    initDescriptedBuffers(bufferInfos);
    //initDescriptedTextures(descriptorSetIndex, imageInfos);

    for(int i = 0; i < HlcSwapchain::MAX_FRAMES_IN_FLIGHT; i++) {
        HlcDescriptorWriter(*descriptorSetLayout, *descriptorPool)
            .writeBuffer(0, &bufferInfos[i])
            .build(descriptorSets[i]);
    }
}

void RenderSystem::initDescriptedBuffers(vector<VkDescriptorBufferInfo>& buffer_infos) {
    for(int i = 0; i < HlcSwapchain::MAX_FRAMES_IN_FLIGHT; i++) {
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


void RenderSystem::createPipelineLayout() {
    vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayout->getDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = push_info::RANGE_COUNT;
    pipelineLayoutInfo.pPushConstantRanges = &push_info::RANGE_INFO;
    if(vkCreatePipelineLayout(hlcDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error(
            "failed to create pipeline layout");
}
void RenderSystem::createPipeline(const VkRenderPass render_pass, const VkSampleCountFlagBits msaa_samples) {
    assert(pipelineLayout != NULL && "create Pipeline: Cannot create pipeline without a layout");
    PipelineConfigInfo pipelineConfig{};
    HlcPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = render_pass;
    pipelineConfig.multisampleInfo.rasterizationSamples = msaa_samples;
    pipelineConfig.pipelineLayout = pipelineLayout;
    hlcPipeline = std::make_unique<HlcPipeline>(hlcDevice, pipelineConfig);
}



void RenderSystem::render(const RenderData& render_data, FrameInfo& frame_info) const {
    hlcPipeline->bind(frame_info.commandBuffer);

    const UniformBuffer uniformBuffer{frame_info.camera.getProjection() * frame_info.camera.getView()};
    descriptedBuffers[frame_info.frameIndex]->writeToBuffer(&uniformBuffer);
    // ReSharper disable once CppNoDiscardExpression
    descriptedBuffers[frame_info.frameIndex]->flush();


    vkCmdBindDescriptorSets(frame_info.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
        &descriptorSets[frame_info.frameIndex], 0, nullptr);

    frame_info.pipelineLayout = pipelineLayout;


    uint32_t index = 0;
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
    }
}
}


//FEATURE create multiple command buffers and a manager
