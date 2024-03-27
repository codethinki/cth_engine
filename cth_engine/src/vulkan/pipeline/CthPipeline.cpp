#include "CthPipeline.hpp"

#include "layout/CthPipelineConfig.hpp"
#include "layout/CthPipelineLayout.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/pipeline/shader/CthShader.hpp"
#include "vulkan//utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>



namespace cth {
using namespace std;
Pipeline::Pipeline(Device* device, const PipelineLayout* pipeline_layout, const GraphicsPipelineConfig& config_info) : device{device} {
    create(config_info, pipeline_layout, nullptr);
}
Pipeline::Pipeline(Device* device, const Pipeline* parent, const GraphicsPipelineConfig& config_info) : device(device) {
    create(config_info, nullptr, parent);
}

Pipeline::~Pipeline() {
    vkDestroyPipeline(device->get(), vkGraphicsPipeline, nullptr);
    log::msg("graphics pipeline destroyed");
}


void Pipeline::create(const GraphicsPipelineConfig& config_info, const PipelineLayout* pipeline_layout, const Pipeline* parent) {
    CTH_ERR(pipeline_layout != nullptr && parent != nullptr, "something went wrong, cannot inherit and specify layout")
        throw details->exception();

    CTH_ERR(pipeline_layout == nullptr && parent == nullptr, "pipeline layout or parent invalid")
        throw details->exception();

    CTH_STABLE_ERR(config_info.renderPass == VK_NULL_HANDLE && parent == nullptr, "renderPass missing in config_info")
        throw details->exception();


    const auto& stages = config_info.shaderStages();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = config_info.vertexInputInfo.get();
    pipelineInfo.pInputAssemblyState = config_info.inputAssemblyInfo.get();
    pipelineInfo.pViewportState = config_info.viewportInfo.get();
    pipelineInfo.pRasterizationState = config_info.rasterizationInfo.get();
    pipelineInfo.pMultisampleState = config_info.multisampleInfo.get();
    pipelineInfo.pColorBlendState = config_info.colorBlendInfo.get();
    pipelineInfo.pDepthStencilState = config_info.depthStencilInfo.get();
    pipelineInfo.pDynamicState = config_info.dynamicStateInfo.get();
    pipelineInfo.renderPass = config_info.renderPass;
    pipelineInfo.subpass = config_info.subpassCount;

    if(pipeline_layout != nullptr) pipelineInfo.layout = pipeline_layout->get();
    else pipelineInfo.basePipelineHandle = parent->vkGraphicsPipeline;

    pipelineInfo.basePipelineIndex = -1; //TODO maybe add support for multi pipeline creation

    const VkResult createResult = vkCreateGraphicsPipelines(device->get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
        &vkGraphicsPipeline);


    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create graphics pipeline")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    cth::log::msg("graphics pipeline created");
}


void Pipeline::bind(VkCommandBuffer command_buffer) const {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline);
}

} //namespace cth
