#pragma once
#include "base_pipeline.hpp"

#include "cvk/utility/constants.hpp"

#include <volk.h>

#include <vector>


namespace cvk {
class Core;

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

    /**
     * create flags
     * @details `ComputePipeline::create()` adds `VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT`
     */
    VkPipelineCreateFlags2 flags = 0;

    /**
     * maps the descriptor sets and bindings of the shader onto the heaps
     * @pre every set and binding of the shader has a mapping
     * @see `DescriptorSpan::mapping()`
     */
    std::vector<VkDescriptorSetAndBindingMappingEXT> descriptorMappings{};
};

/**
 * compute `VkPipeline` wrapper
 */
class ComputePipeline : public BasePipeline {
public:
    using Config = ComputePipelineConfig;

    /**
     * base constructor
     */
    ComputePipeline(Core const&, Config);

    /**
     * constructs and wraps
     * @details calls @ref ComputePipeline(Core const&, Config), `wrap()`
     */
    ComputePipeline(Core const&, Config, State const&);

    /**
     * constructs and creates
     * @details calls @ref ComputePipeline(Core const&, Config), `create()`
     */
    explicit ComputePipeline(Core const&, Config, create_t);

    /**
     * creates the pipeline
     * @details chains the descriptor mappings to the stage
     * @pre `Core::created()`
     * @pre the stage of the config is a compute shader
     * @pre the shader module of the config is not destroyed
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
