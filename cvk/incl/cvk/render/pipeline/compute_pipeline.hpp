#pragma once
#include "base_pipeline.hpp"

#include "cvk/utility/constants.hpp"

#include <volk.h>


namespace cvk {
class PipelineLayout;

/**
 * config of a @ref ComputePipeline
 */
struct ComputePipelineConfig {
    /**
     * stage of a compute shader, see @ref Shader::stageInfo()
     * @attention the specialization info and the entry point name outlive @ref ComputePipeline::create()
     */
    VkPipelineShaderStageCreateInfo stage{};

    VkPipelineCreateFlags flags = 0;
};

/**
 * compute VkPipeline wrapper
 */
class ComputePipeline : public BasePipeline {
public:
    using Config = ComputePipelineConfig;

    /**
     * base constructor, keeps the layout
     * @pre @ref Core::created() of the layout
     */
    ComputePipeline(PipelineLayout const&, Config const&);

    /**
     * constructs and wraps
     * @details calls @ref ComputePipeline(PipelineLayout const&, Config const&), @ref wrap()
     */
    ComputePipeline(PipelineLayout const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref ComputePipeline(PipelineLayout const&, Config const&), @ref create()
     */
    explicit ComputePipeline(PipelineLayout const&, Config const&, create_t);

    /**
     * creates the pipeline
     * @pre the stage of the config is a compute shader
     * @pre the layout and the shader module of the config are not destroyed
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateComputePipelines()
     */
    void create();

    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    Config _config;
};
}
