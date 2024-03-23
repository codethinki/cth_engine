#include "CthPipeline.hpp"

#include "CthDevice.hpp"
#include "../model/HlcVertex.hpp"
#include "../utils/cth_vk_specific_utils.hpp"
#include "..\utils\CthShader.hpp"

#include <cth/cth_log.hpp>


#include <cassert>
#include <stdexcept>



namespace cth {
using namespace std;

Pipeline::Pipeline(Device* device, const PipelineConfigInfo& config_info) : device{device} { createGraphicsPipeline(config_info); }
Pipeline::~Pipeline() {
    vkDestroyPipeline(device->device(), vkGraphicsPipeline, nullptr);
}

void Pipeline::createGraphicsPipeline(const PipelineConfigInfo& config_info) {
    CTH_STABLE_ERR(config_info.pipelineLayout == VK_NULL_HANDLE, "pipelineLayout missing in config_info")
        throw cth::except::data_exception{config_info, details->exception()};
    CTH_STABLE_ERR(config_info.renderPass == VK_NULL_HANDLE, "renderPass missing in config_info")
        throw cth::except::data_exception{config_info, details->exception()};

    array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = device->vertShader->get();
    shaderStages[0].pName = "main";
    shaderStages[0].flags = 0;
    shaderStages[0].pNext = nullptr;
    shaderStages[0].pSpecializationInfo = nullptr;

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = device->fragShader->get();
    shaderStages[1].pName = "main";
    shaderStages[1].flags = 0;
    shaderStages[1].pNext = nullptr;
    shaderStages[1].pSpecializationInfo = nullptr;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(VERTEX_ATTRIBUTE_DESCRIPTIONS.size());
    vertexInputInfo.pVertexAttributeDescriptions = VERTEX_ATTRIBUTE_DESCRIPTIONS.data();

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(VERTEX_BINDING_DESCRIPTIONS.size());
    vertexInputInfo.pVertexBindingDescriptions = VERTEX_BINDING_DESCRIPTIONS.data();


    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &config_info.inputAssemblyInfo;
    pipelineInfo.pViewportState = &config_info.viewportInfo;
    pipelineInfo.pRasterizationState = &config_info.rasterizationInfo;
    pipelineInfo.pMultisampleState = &config_info.multisampleInfo;
    pipelineInfo.pColorBlendState = &config_info.colorBlendInfo;
    pipelineInfo.pDepthStencilState = &config_info.depthStencilInfo;
    pipelineInfo.pDynamicState = &config_info.dynamicStateInfo;
    pipelineInfo.layout = config_info.pipelineLayout;
    pipelineInfo.renderPass = config_info.renderPass;
    pipelineInfo.subpass = config_info.subpassCount;
    pipelineInfo.basePipelineIndex = -1;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    const VkResult createResult = vkCreateGraphicsPipelines(device->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
        &vkGraphicsPipeline);


    CTH_STABLE_ERR(createResult != VK_SUCCESS, "Vk: failed to create graphics pipeline")
        throw cth::except::vk_result_exception{createResult, details->exception()};


}


void Pipeline::bind(VkCommandBuffer command_buffer) const { vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline); }


void Pipeline::defaultPipelineConfigInfo(PipelineConfigInfo& config_info) {

    config_info.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    config_info.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config_info.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    config_info.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    config_info.viewportInfo.viewportCount = 1;
    config_info.viewportInfo.pViewports = nullptr;
    config_info.viewportInfo.scissorCount = 1;
    config_info.viewportInfo.pScissors = nullptr;

    config_info.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    config_info.rasterizationInfo.depthClampEnable = VK_FALSE;
    config_info.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    config_info.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    config_info.rasterizationInfo.lineWidth = 1.0f;
    config_info.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    config_info.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    config_info.rasterizationInfo.depthBiasEnable = VK_FALSE;
    config_info.rasterizationInfo.depthBiasConstantFactor = 0.0f; // Optional
    config_info.rasterizationInfo.depthBiasClamp = 0.0f; // Optional
    config_info.rasterizationInfo.depthBiasSlopeFactor = 0.0f; // Optional

    config_info.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    config_info.multisampleInfo.sampleShadingEnable = VK_FALSE;
    config_info.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    config_info.multisampleInfo.minSampleShading = 0.f; // Optional
    config_info.multisampleInfo.pSampleMask = nullptr; // Optional
    config_info.multisampleInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    config_info.multisampleInfo.alphaToOneEnable = VK_FALSE; // Optional

    config_info.colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    config_info.colorBlendAttachment.blendEnable = VK_TRUE;
    config_info.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
    config_info.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    config_info.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    config_info.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    config_info.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    config_info.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    config_info.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    config_info.colorBlendInfo.logicOpEnable = VK_FALSE;
    config_info.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
    config_info.colorBlendInfo.attachmentCount = 1;
    config_info.colorBlendInfo.pAttachments = &config_info.colorBlendAttachment;
    config_info.colorBlendInfo.blendConstants[0] = 0.0f; // Optional
    config_info.colorBlendInfo.blendConstants[1] = 0.0f; // Optional
    config_info.colorBlendInfo.blendConstants[2] = 0.0f; // Optional
    config_info.colorBlendInfo.blendConstants[3] = 0.0f; // Optional

    config_info.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    config_info.depthStencilInfo.depthTestEnable = VK_TRUE;
    config_info.depthStencilInfo.depthWriteEnable = VK_TRUE;
    config_info.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    config_info.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    config_info.depthStencilInfo.minDepthBounds = 0.0f; // Optional
    config_info.depthStencilInfo.maxDepthBounds = 1.0f; // Optional
    config_info.depthStencilInfo.stencilTestEnable = VK_FALSE;
    config_info.depthStencilInfo.front = {}; // Optional
    config_info.depthStencilInfo.back = {}; // Optional

    config_info.dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    config_info.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    config_info.dynamicStateInfo.pDynamicStates = config_info.dynamicStates.data();
    config_info.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(config_info.dynamicStates.size());
    config_info.dynamicStateInfo.flags = 0;
}
}
