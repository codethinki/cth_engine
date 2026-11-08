#pragma once
#include "base_pipeline.hpp"

#include "cvk/utility/constants.hpp"

#include <volk.h>


namespace cvk {
class PipelineLayout;

/**
 * config of a `ComputePipeline`
 */
struct ComputePipelineConfig {
    /**
     * stage of a compute shader
     * @attention the specialization info and the entry point name outlive `ComputePipeline::create()`
     * @see `Shader::stageInfo()`
     */
    VkPipelineShaderStageCreateInfo stage{};

    VkPipelineCreateFlags flags = 0;
};

/**
 * compute `VkPipeline` wrapper
 */
class ComputePipeline : public BasePipeline {
public:
    using Config = ComputePipelineConfig;

    /**
     * base constructor, keeps the layout
     * @pre `Core::created()` of the layout
     */
    ComputePipeline(PipelineLayout const&, Config const&);

    /**
     * constructs and wraps
     * @details calls `ComputePipeline(PipelineLayout const&, Config const&)`, `wrap()`
     */
    ComputePipeline(PipelineLayout const&, Config const&, State const&);

    /**
     * constructs and creates
     * @details calls `ComputePipeline(PipelineLayout const&, Config const&)`, `create()`
     */
    explicit ComputePipeline(PipelineLayout const&, Config const&, create_t);

    /**
     * creates the pipeline
     * @pre the stage of the config is a compute shader
     * @pre the layout and the shader module of the config are not destroyed
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateComputePipelines()`
     */
    void create();

    [[nodiscard]] constexpr Config const& config() const { return _config; }

private:
    Config _config;
};
}
