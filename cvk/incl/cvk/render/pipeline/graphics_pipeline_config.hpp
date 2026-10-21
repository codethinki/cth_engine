#pragma once
#include "cvk/render/cmd/rendering_info.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <vector>


namespace cvk {
class Shader;

/**
 * @brief graphics pipeline state, the defaults are the legacy defaults
 * @details @ref GraphicsPipeline::create() sets the pointer and count members of the Vulkan structs from the vectors
 */
struct GraphicsPipelineConfig {
    /**
     * @brief alpha blending: src alpha, one minus src alpha
     */
    static constexpr VkPipelineColorBlendAttachmentState ALPHA_BLENDING{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT,
    };

    /**
     * @brief adds the stage of the shader
     * @param specialization nullptr -> no specialization constants
     * @attention @ref specialization must outlive @ref GraphicsPipeline::create()
     * @pre @ref Shader::created()
     * @pre no stage of the same type
     */
    GraphicsPipelineConfig& addStage(Shader const&, VkSpecializationInfo const* specialization = nullptr);

    /**
     * @pre a stage of the type exists
     */
    constexpr GraphicsPipelineConfig& removeStage(VkShaderStageFlagBits stage);

    /**
     * @brief VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT -> the pipeline can be a parent
     */
    VkPipelineCreateFlags flags = 0;

    std::vector<VkPipelineShaderStageCreateInfo> stages{};

    std::vector<VkVertexInputBindingDescription> vertexBindings{};
    std::vector<VkVertexInputAttributeDescription> vertexAttributes{};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineRasterizationStateCreateInfo rasterization{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.f,
        .depthBiasClamp = 0.f,
        .depthBiasSlopeFactor = 0.f,
        .lineWidth = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.f,
        .maxDepthBounds = 1.f,
    };

    /**
     * @brief color blend state, attachmentCount and pAttachments come from @ref colorBlendAttachments
     */
    VkPipelineColorBlendStateCreateInfo colorBlend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 0,
        .pAttachments = nullptr,
        .blendConstants = {0.f, 0.f, 0.f, 0.f},
    };

    /**
     * @brief blend state per color attachment, empty -> @ref ALPHA_BLENDING per color format
     * @pre empty or size == @ref colorFormats size
     */
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments{};

    uint32_t viewportCount = 1;
    uint32_t scissorCount = 1;

    std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    /**
     * @brief formats of the color attachments of the rendering
     */
    std::vector<VkFormat> colorFormats{};
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
    uint32_t viewMask = 0;

    /**
     * @brief == the mapping of the command buffer at a draw, see @ref BaseCmdBuffer::setAttachmentMapping()
     * @pre @ref AttachmentMapping::debug_check() passes for @ref colorFormats
     */
    AttachmentMapping attachmentMapping{};
};
}

namespace cvk {
constexpr GraphicsPipelineConfig& GraphicsPipelineConfig::removeStage(VkShaderStageFlagBits stage) {
    auto const removed = std::erase_if(
        stages,
        [stage](VkPipelineShaderStageCreateInfo const& info) { return info.stage == stage; }
    );
    CTH_CRITICAL(removed == 0, "stage not added") {}

    return *this;
}
}
