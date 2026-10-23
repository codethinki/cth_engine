#include "cvk/render/pipeline/graphics_pipeline.hpp"

#include "cvk/base/core.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
GraphicsPipeline::GraphicsPipeline(PipelineLayout const& layout, Config config) :
    BasePipeline{layout, VK_PIPELINE_BIND_POINT_GRAPHICS}, _config{std::move(config)} {}

GraphicsPipeline::GraphicsPipeline(PipelineLayout const& layout, Config config, State const& state) :
    GraphicsPipeline{layout, std::move(config)} {
    wrap(state);
}

GraphicsPipeline::GraphicsPipeline(PipelineLayout const& layout, Config config, create_t) :
    GraphicsPipeline{layout, std::move(config)} {
    create();
}

GraphicsPipeline::GraphicsPipeline(GraphicsPipeline const& parent, Config config) :
    BasePipeline{parent.layout(), VK_PIPELINE_BIND_POINT_GRAPHICS},
    _vkParent{parent.get()},
    _config{std::move(config)} {
    debug_check(parent);
    CTH_CRITICAL(
        (parent.config().flags & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT) == 0,
        "parent must allow derivatives (VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT)"
    ) {}
}

GraphicsPipeline::GraphicsPipeline(GraphicsPipeline const& parent, Config config, create_t) :
    GraphicsPipeline{parent, std::move(config)} {
    create();
}

void GraphicsPipeline::create() {
    CTH_CRITICAL(
        !_config.colorBlendAttachments.empty() && _config.colorBlendAttachments.size() != _config.colorFormats.size(),
        "one color blend attachment per color format required, got {} for {}",
        _config.colorBlendAttachments.size(),
        _config.colorFormats.size()
    ) {}
    AttachmentMapping::debug_check(_config.attachmentMapping, _config.colorFormats.size());

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

    std::vector<VkPipelineColorBlendAttachmentState> const defaultBlendAttachments(
        config.colorBlendAttachments.empty() ? config.colorFormats.size() : 0,
        Config::ALPHA_BLENDING
    );
    auto const& blendAttachments = config.colorBlendAttachments.empty()
        ? defaultBlendAttachments
        : config.colorBlendAttachments;

    auto colorBlend = config.colorBlend;
    colorBlend.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    colorBlend.pAttachments = blendAttachments.data();

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
        .layout = layout().get(),
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = _vkParent,
        .basePipelineIndex = -1,
    };

    BasePipeline::create(info);
}
}
