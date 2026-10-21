#include "cvk/render/pipeline/compute_pipeline.hpp"

#include "cvk/render/pipeline/layout/pipeline_layout.hpp"


namespace cvk {
ComputePipeline::ComputePipeline(Core const& core, PipelineLayout const& layout, Config const& config) :
    BasePipeline{core, layout, VK_PIPELINE_BIND_POINT_COMPUTE}, _config{config} {}

ComputePipeline::ComputePipeline(
    Core const& core,
    PipelineLayout const& layout,
    Config const& config,
    State const& state
) : ComputePipeline{core, layout, config} { wrap(state); }

ComputePipeline::ComputePipeline(Core const& core, PipelineLayout const& layout, Config const& config, create_t) :
    ComputePipeline{core, layout, config} {
    create();
}

void ComputePipeline::create() {
    CTH_CRITICAL(_config.stage.stage != VK_SHADER_STAGE_COMPUTE_BIT, "the stage must be a compute shader") {}
    CTH_CRITICAL(_config.stage.module == VK_NULL_HANDLE, "the stage needs a shader module") {}

    VkComputePipelineCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = _config.flags,
        .stage = _config.stage,
        .layout = layout().get(),
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    BasePipeline::create(info);
}
}
