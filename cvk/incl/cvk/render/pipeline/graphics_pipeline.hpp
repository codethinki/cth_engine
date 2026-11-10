#pragma once
#include "base_pipeline.hpp"
#include "graphics_pipeline_config.hpp"

#include "cvk/utility/constants.hpp"

#include <volk.h>


namespace cvk {
class PipelineLayout;

/**
 * graphics `VkPipeline` wrapper
 * @details for dynamic rendering
 */
class GraphicsPipeline : public BasePipeline {
public:
    using Config = GraphicsPipelineConfig;

    /**
     * base constructor, keeps the layout
     */
    GraphicsPipeline(PipelineLayout const&, Config);

    /**
     * constructs and wraps
     * @details calls @ref GraphicsPipeline(PipelineLayout const&, Config), `wrap()`
     */
    GraphicsPipeline(PipelineLayout const&, Config, State const&);

    /**
     * constructs and creates
     * @details calls @ref GraphicsPipeline(PipelineLayout const&, Config), `create()`
     */
    explicit GraphicsPipeline(PipelineLayout const&, Config, create_t);

    /**
     * base constructor of a derivative pipeline, keeps the layout and copies the handle of the parent
     * @pre `created()` of the parent, with `VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT` in its flags
     */
    GraphicsPipeline(GraphicsPipeline const& parent, Config);

    /**
     * constructs and creates a derivative pipeline
     * @details calls @ref GraphicsPipeline(GraphicsPipeline const&, Config), `create()`
     */
    explicit GraphicsPipeline(GraphicsPipeline const& parent, Config, create_t);

    /**
     * creates the pipeline
     * @details derivative pipeline -> adds `VK_PIPELINE_CREATE_DERIVATIVE_BIT`
     * @details empty `Config::colorBlendAttachments` -> `Config::ALPHA_BLENDING` per color format
     * @pre the layout, the parent and the shader modules of the config are not destroyed
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateGraphicsPipelines()`
     */
    void create();

    /**
     * @return `VK_NULL_HANDLE` -> not a derivative pipeline
     */
    [[nodiscard]] constexpr VkPipeline parent() const { return _vkParent; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    VkPipeline _vkParent = VK_NULL_HANDLE;
    Config _config;
};
}
