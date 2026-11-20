#pragma once
#include "base_pipeline.hpp"
#include "graphics_pipeline_config.hpp"

#include "cvk/utility/constants.hpp"

#include <volk.h>


namespace cvk {
class Core;

/**
 * graphics `VkPipeline` wrapper
 * @details for dynamic rendering
 */
class GraphicsPipeline : public BasePipeline {
public:
    using Config = GraphicsPipelineConfig;

    /**
     * base constructor
     */
    GraphicsPipeline(Core const&, Config);

    /**
     * constructs and wraps
     * @details calls @ref GraphicsPipeline(Core const&, Config), `wrap()`
     */
    GraphicsPipeline(Core const&, Config, State const&);

    /**
     * constructs and creates
     * @details calls @ref GraphicsPipeline(Core const&, Config), `create()`
     */
    explicit GraphicsPipeline(Core const&, Config, create_t);

    /**
     * base constructor of a derivative pipeline, keeps the parent
     */
    GraphicsPipeline(GraphicsPipeline const& parent, Config);

    /**
     * constructs and creates a derivative pipeline
     * @details calls @ref GraphicsPipeline(GraphicsPipeline const&, Config), `create()`
     */
    explicit GraphicsPipeline(GraphicsPipeline const& parent, Config, create_t);

    /**
     * creates the pipeline
     * @details derivative pipeline -> adds `VK_PIPELINE_CREATE_2_DERIVATIVE_BIT`, the parent is the base pipeline
     * @details empty `Config::colorBlendAttachments` -> `Config::ALPHA_BLENDING` per color format
     * @details chains the descriptor mappings to every stage
     * @pre `Core::created()`
     * @pre the shader modules of the config are not destroyed
     * @pre derivative pipeline -> `created()` of the parent, with `VK_PIPELINE_CREATE_2_ALLOW_DERIVATIVES_BIT` in
     *  its flags
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateGraphicsPipelines()`
     */
    void create();

    /**
     * @return `nullptr` -> not a derivative pipeline
     */
    [[nodiscard]] constexpr GraphicsPipeline const* parent() const { return _parentPtr; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    GraphicsPipeline const* _parentPtr = nullptr; ///< `nullptr` -> not a derivative pipeline
    Config _config;
};
}
