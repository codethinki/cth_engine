#include "cvk/render/pipeline/pipeline.hpp"

#include "cvk/base/core.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
Pipeline::Pipeline(Core const& core, PipelineLayout const& layout, Config config) : _deviceTable{core.deviceTable()},
    _vkLayout{layout.get()},
    _config{std::move(config)} {}

Pipeline::Pipeline(Core const& core, PipelineLayout const& layout, Config config, State const& state) : Pipeline{
    core,
    layout,
    std::move(config)
} { wrap(state); }

Pipeline::Pipeline(Core const& core, PipelineLayout const& layout, Config config, create_t) : Pipeline{
    core,
    layout,
    std::move(config)
} { create(); }

void Pipeline::wrap(State const& state) {
    optDestroy();

    _handle = state.vkPipeline.get();
}

void Pipeline::create() {
    CTH_CRITICAL(
        _config.colorBlendAttachments.size() != _config.colorFormats.size(),
        "one color blend attachment per color format required"
    ) {}

    optDestroy();

    auto const& config = _config;

    VkPipelineVertexInputStateCreateInfo const vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = static_cast<uint32_t>(config.vertexBindings.size()),
        .pVertexBindingDescriptions = config.vertexBindings.data(),
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertexAttributes.size()),
        .pVertexAttributeDescriptions = config.vertexAttributes.data(),
    };

    VkPipelineViewportStateCreateInfo const viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = config.viewportCount,
        .pViewports = nullptr,
        .scissorCount = config.scissorCount,
        .pScissors = nullptr,
    };

    auto colorBlend = config.colorBlend;
    colorBlend.attachmentCount = static_cast<uint32_t>(config.colorBlendAttachments.size());
    colorBlend.pAttachments = config.colorBlendAttachments.data();

    VkPipelineDynamicStateCreateInfo const dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(config.dynamicStates.size()),
        .pDynamicStates = config.dynamicStates.data(),
    };

    VkPipelineRenderingCreateInfo const rendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = config.viewMask,
        .colorAttachmentCount = static_cast<uint32_t>(config.colorFormats.size()),
        .pColorAttachmentFormats = config.colorFormats.data(),
        .depthAttachmentFormat = config.depthFormat,
        .stencilAttachmentFormat = config.stencilFormat,
    };

    VkGraphicsPipelineCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(config.stages.size()),
        .pStages = config.stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &config.inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport,
        .pRasterizationState = &config.rasterization,
        .pMultisampleState = &config.multisample,
        .pDepthStencilState = &config.depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = _vkLayout.get(),
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkPipeline ptr = VK_NULL_HANDLE;
    auto const result = _deviceTable->vkCreateGraphicsPipelines(
        _deviceTable.device(),
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &ptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create graphics pipeline") {
        reset();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle = ptr;
}

void Pipeline::destroy() {
    debug_check(*this);

    destroy(_deviceTable, _handle.get());

    reset();
}

Pipeline::State Pipeline::release() {
    debug_check(*this);

    State const state{_handle.get()};
    reset();
    return state;
}

void Pipeline::destroy(DeviceTable table, VkPipeline vkPipeline) {
    CTH_WARN(vkPipeline == VK_NULL_HANDLE, "vkPipeline invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipeline(table.device(), vkPipeline, nullptr);
}

void Pipeline::reset() { _handle = VK_NULL_HANDLE; }
}
