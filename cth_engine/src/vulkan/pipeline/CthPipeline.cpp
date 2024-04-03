#include "CthPipeline.hpp"

#include "layout/CthPipelineLayout.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/pipeline/shader/CthShader.hpp"
#include "vulkan//utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include "vulkan/render/model/CthVertex.hpp"



namespace cth {
using namespace std;
Pipeline::Pipeline(Device* device, const PipelineLayout* pipeline_layout, const GraphicsConfig& config_info) : device{device} {
    create(config_info, pipeline_layout, nullptr);
}
Pipeline::Pipeline(Device* device, const Pipeline* parent, const GraphicsConfig& config_info) : device(device) {
    create(config_info, nullptr, parent);
}

Pipeline::~Pipeline() {
    vkDestroyPipeline(device->get(), vkGraphicsPipeline, nullptr);
    log::msg("destroyed graphics-pipeline ");
}

void Pipeline::bind(VkCommandBuffer command_buffer) const { vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline); }

void Pipeline::create(const GraphicsConfig& config_info, const PipelineLayout* pipeline_layout, const Pipeline* parent) {
    CTH_ERR(pipeline_layout != nullptr && parent != nullptr, "something went wrong, cannot inherit and specify layout")
        throw details->exception();

    CTH_ERR(pipeline_layout == nullptr && parent == nullptr, "pipeline layout or parent invalid")
        throw details->exception();

    CTH_STABLE_ERR(config_info.renderPass == VK_NULL_HANDLE && parent == nullptr, "renderPass missing in config_info")
        throw details->exception();


    auto pipelineInfo = config_info.createInfo();

    if(pipeline_layout != nullptr) pipelineInfo.layout = pipeline_layout->get();
    else pipelineInfo.basePipelineHandle = parent->vkGraphicsPipeline;


    const VkResult createResult = vkCreateGraphicsPipelines(device->get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
        &vkGraphicsPipeline);


    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create graphics pipeline")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    cth::log::msg("created graphics-pipeline ");
}


} //namespace cth

//Graphics Config

namespace cth {
VkGraphicsPipelineCreateInfo Pipeline::GraphicsConfig::createInfo() const {
    VkGraphicsPipelineCreateInfo pipelineInfo{};

    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(vkShaderStages.size());
    if(!vkShaderStages.empty()) pipelineInfo.pStages = vkShaderStages.data();
    else pipelineInfo.pStages = nullptr;
    pipelineInfo.pVertexInputState = vertexInputInfo.get();
    pipelineInfo.pInputAssemblyState = inputAssemblyInfo.get();
    pipelineInfo.pViewportState = viewportInfo.get();
    pipelineInfo.pRasterizationState = rasterizationInfo.get();
    pipelineInfo.pMultisampleState = multisampleInfo.get();
    pipelineInfo.pColorBlendState = colorBlendInfo.get();
    pipelineInfo.pDepthStencilState = depthStencilInfo.get();
    pipelineInfo.pDynamicState = dynamicStateInfo.get();
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = subpassCount;

    pipelineInfo.basePipelineIndex = -1; //FEATURE maybe use this


    return pipelineInfo;
}

void Pipeline::GraphicsConfig::addShaderStage(const Shader* shader, const ShaderSpecialization* specialization_info,
    const VkPipelineShaderStageCreateFlags flags) {
    VkPipelineShaderStageCreateInfo stageInfo{};

    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pName = "main";
    stageInfo.flags = flags;
    stageInfo.stage = shader->stage();
    stageInfo.module = shader->module();
    if(specialization_info != nullptr) stageInfo.pSpecializationInfo = specialization_info->get();

    vkShaderStages.push_back(stageInfo);
}
void Pipeline::GraphicsConfig::removeShaderStage(const Shader* shader) { removeShaderStage(shader->stage()); }
void Pipeline::GraphicsConfig::removeShaderStage(VkShaderStageFlagBits shader_stage) {
    const auto it = ranges::find_if(vkShaderStages,
        [shader_stage](const VkPipelineShaderStageCreateInfo& info) { return info.stage == shader_stage; });

    if(it != vkShaderStages.end()) vkShaderStages.erase(it);
    else
        CTH_ERR(true, "non present shader stage removed") {
            details->add("stage: {}", static_cast<uint32_t>(shader_stage));
            throw details->exception();
        }
}


void Pipeline::GraphicsConfig::setDefault(GraphicsConfig& config) {
    config.vertexInputInfo = make_unique<VkPipelineVertexInputStateCreateInfo>();
    config.vertexInputInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    config.vertexInputInfo->vertexAttributeDescriptionCount = static_cast<uint32_t>(VERTEX_ATTRIBUTE_DESCRIPTIONS.size());
    config.vertexInputInfo->pVertexAttributeDescriptions = VERTEX_ATTRIBUTE_DESCRIPTIONS.data();

    config.vertexInputInfo->vertexBindingDescriptionCount = static_cast<uint32_t>(VERTEX_BINDING_DESCRIPTIONS.size());
    config.vertexInputInfo->pVertexBindingDescriptions = VERTEX_BINDING_DESCRIPTIONS.data();

    config.inputAssemblyInfo = make_unique<VkPipelineInputAssemblyStateCreateInfo>();
    config.inputAssemblyInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    config.inputAssemblyInfo->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.inputAssemblyInfo->primitiveRestartEnable = VK_FALSE;

    config.rasterizationInfo = make_unique<VkPipelineRasterizationStateCreateInfo>();
    config.rasterizationInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    config.rasterizationInfo->depthClampEnable = VK_FALSE;
    config.rasterizationInfo->rasterizerDiscardEnable = VK_FALSE;
    config.rasterizationInfo->polygonMode = VK_POLYGON_MODE_FILL;
    config.rasterizationInfo->lineWidth = 1.f;
    config.rasterizationInfo->cullMode = VK_CULL_MODE_NONE;
    config.rasterizationInfo->frontFace = VK_FRONT_FACE_CLOCKWISE;
    config.rasterizationInfo->depthBiasEnable = VK_FALSE;
    config.rasterizationInfo->depthBiasConstantFactor = 0.f; // Optional
    config.rasterizationInfo->depthBiasClamp = 0.f; // Optional
    config.rasterizationInfo->depthBiasSlopeFactor = 0.0f; // Optional

    config.multisampleInfo = make_unique<VkPipelineMultisampleStateCreateInfo>();
    config.multisampleInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    config.multisampleInfo->sampleShadingEnable = VK_FALSE;
    config.multisampleInfo->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    config.multisampleInfo->minSampleShading = 0.f; // Optional
    config.multisampleInfo->pSampleMask = nullptr; // Optional
    config.multisampleInfo->alphaToCoverageEnable = VK_FALSE; // Optional
    config.multisampleInfo->alphaToOneEnable = VK_FALSE; // Optional

    config.colorBlendAttachment = make_unique<VkPipelineColorBlendAttachmentState>();
    config.colorBlendAttachment->colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    config.colorBlendAttachment->blendEnable = VK_TRUE;
    config.colorBlendAttachment->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
    config.colorBlendAttachment->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    config.colorBlendAttachment->colorBlendOp = VK_BLEND_OP_ADD; // Optional
    config.colorBlendAttachment->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    config.colorBlendAttachment->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    config.colorBlendAttachment->alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    config.colorBlendInfo = make_unique<VkPipelineColorBlendStateCreateInfo>();
    config.colorBlendInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    config.colorBlendInfo->logicOpEnable = VK_FALSE;
    config.colorBlendInfo->logicOp = VK_LOGIC_OP_COPY; // Optional
    config.colorBlendInfo->attachmentCount = 1;
    config.colorBlendInfo->pAttachments = config.colorBlendAttachment.get();
    config.colorBlendInfo->blendConstants[0] = 0.0f; // Optional
    config.colorBlendInfo->blendConstants[1] = 0.0f; // Optional
    config.colorBlendInfo->blendConstants[2] = 0.0f; // Optional
    config.colorBlendInfo->blendConstants[3] = 0.0f; // Optional

    config.depthStencilInfo = make_unique<VkPipelineDepthStencilStateCreateInfo>();
    config.depthStencilInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    config.depthStencilInfo->depthTestEnable = VK_TRUE;
    config.depthStencilInfo->depthWriteEnable = VK_TRUE;
    config.depthStencilInfo->depthCompareOp = VK_COMPARE_OP_LESS;
    config.depthStencilInfo->depthBoundsTestEnable = VK_FALSE;
    config.depthStencilInfo->minDepthBounds = .0f; // Optional
    config.depthStencilInfo->maxDepthBounds = 1.0f; // Optional
    config.depthStencilInfo->stencilTestEnable = VK_FALSE;
    config.depthStencilInfo->front = {}; // Optional
    config.depthStencilInfo->back = {}; // Optional


    config.viewportInfo = make_unique<VkPipelineViewportStateCreateInfo>();
    config.viewportInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    config.viewportInfo->viewportCount = 1;
    config.viewportInfo->pViewports = nullptr;
    config.viewportInfo->scissorCount = 1;
    config.viewportInfo->pScissors = nullptr;

    config.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    config.dynamicStateInfo = make_unique<VkPipelineDynamicStateCreateInfo>();
    config.dynamicStateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    config.dynamicStateInfo->pDynamicStates = config.dynamicStates.data();
    config.dynamicStateInfo->dynamicStateCount = static_cast<uint32_t>(config.dynamicStates.size());
    config.dynamicStateInfo->flags = 0;
}
Pipeline::GraphicsConfig Pipeline::GraphicsConfig::createDefault() {
    GraphicsConfig config;
    setDefault(config);
    return config;
}
}
