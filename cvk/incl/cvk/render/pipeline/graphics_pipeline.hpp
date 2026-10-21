#pragma once
#include "base_pipeline.hpp"
#include "graphics_pipeline_config.hpp"

#include "cvk/utility/constants.hpp"

#include <volk.h>


namespace cvk {
class Core;
class PipelineLayout;

/**
 * @brief graphics VkPipeline wrapper for dynamic rendering
 */
class GraphicsPipeline : public BasePipeline {
public:
    using Config = GraphicsPipelineConfig;

    /**
     * @brief base constructor, keeps the layout
     * @pre @ref Core::created()
     */
    GraphicsPipeline(Core const&, PipelineLayout const&, Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref GraphicsPipeline(Core const&, PipelineLayout const&, Config)
     * - @ref wrap()
     */
    GraphicsPipeline(Core const&, PipelineLayout const&, Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref GraphicsPipeline(Core const&, PipelineLayout const&, Config)
     * - @ref create()
     */
    explicit GraphicsPipeline(Core const&, PipelineLayout const&, Config, create_t);

    /**
     * @brief base constructor of a derivative pipeline, keeps the layout and copies the handle of the parent
     * @pre @ref Core::created()
     * @pre @ref created() of the parent, with VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT in its flags
     */
    GraphicsPipeline(Core const&, GraphicsPipeline const& parent, Config);

    /**
     * @brief constructs and creates a derivative pipeline
     * @details calls:
     * - @ref GraphicsPipeline(Core const&, GraphicsPipeline const&, Config)
     * - @ref create()
     */
    explicit GraphicsPipeline(Core const&, GraphicsPipeline const& parent, Config, create_t);

    /**
     * @brief creates the pipeline
     * @details derivative pipeline -> adds VK_PIPELINE_CREATE_DERIVATIVE_BIT
     * @pre the layout, the parent and the shader modules of the config are not destroyed
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateGraphicsPipelines()
     */
    void create();

    /**
     * @return VK_NULL_HANDLE -> not a derivative pipeline
     */
    [[nodiscard]] constexpr VkPipeline parent() const { return _vkParent; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    VkPipeline _vkParent = VK_NULL_HANDLE;
    Config _config;
};
}
