#include "cvk/render/pipeline/pipeline.hpp"

#include "cvk/base/core.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
Pipeline::Pipeline(Core const& core, PipelineLayout const& layout, Config config) : _handle{core},
    _layout{&layout},
    _config{std::move(config)} { Core::debug_check(core); }

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

Pipeline::Pipeline(Core const& core, Pipeline const& parent, Config config) : _handle{core},
    _layout{&parent.layout()},
    _vkParent{parent.get()},
    _config{std::move(config)} {
    Core::debug_check(core);
    debug_check(parent);
    CTH_CRITICAL(
        (parent.config().flags & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT) == 0,
        "parent must allow derivatives (VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT)"
    ) {}
}

Pipeline::Pipeline(Core const& core, Pipeline const& parent, Config config, create_t) : Pipeline{
    core,
    parent,
    std::move(config)
} { create(); }

void Pipeline::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkPipeline.get(), core());
}

void Pipeline::create() {
    PipelineLayout::debug_check(*_layout);
    CTH_CRITICAL(
        _config.colorBlendAttachments.size() != _config.colorFormats.size(),
        "one color blend attachment per color format required"
    ) {}
    AttachmentMapping::debug_check(_config.attachmentMapping, _config.colorFormats.size());

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

    auto const colorAttachments = static_cast<uint32_t>(config.colorFormats.size());
    auto const inputIndices = config.attachmentMapping.inputIndexInfo(colorAttachments);
    auto locations = config.attachmentMapping.locationInfo(colorAttachments);
    locations.pNext = &inputIndices;

    VkPipelineRenderingCreateInfo const rendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = &locations,
        .viewMask = config.viewMask,
        .colorAttachmentCount = colorAttachments,
        .pColorAttachmentFormats = config.colorFormats.data(),
        .depthAttachmentFormat = config.depthFormat,
        .stencilAttachmentFormat = config.stencilFormat,
    };

    VkGraphicsPipelineCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .flags = config.flags | (_vkParent != VK_NULL_HANDLE ? VK_PIPELINE_CREATE_DERIVATIVE_BIT : 0u),
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
        .layout = _layout->get(),
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = _vkParent,
        .basePipelineIndex = -1,
    };

    VkPipeline ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateGraphicsPipelines(
        core().vkDevice(),
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &ptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create graphics pipeline") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void Pipeline::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Pipeline::State Pipeline::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void Pipeline::destroy(DeviceTable table, VkPipeline vkPipeline) {
    CTH_WARN(vkPipeline == VK_NULL_HANDLE, "vkPipeline invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipeline(table.device(), vkPipeline, nullptr);
}

void Pipeline::forget() {}
}
