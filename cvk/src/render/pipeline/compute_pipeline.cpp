#include "cvk/render/pipeline/compute_pipeline.hpp"

#include <utility>


namespace cvk {
ComputePipeline::ComputePipeline(Core const& core, Config config) : BasePipeline{core, VK_PIPELINE_BIND_POINT_COMPUTE},
    _config{std::move(config)} {}

ComputePipeline::ComputePipeline(Core const& core, Config config, State const& state) :
    ComputePipeline{core, std::move(config)} {
    wrap(state);
}

ComputePipeline::ComputePipeline(Core const& core, Config config, create_t) : ComputePipeline{core, std::move(config)} {
    create();
}

void ComputePipeline::create() {
    CTH_CRITICAL(_config.stage.stage != VK_SHADER_STAGE_COMPUTE_BIT, "the stage must be a compute shader") {}
    CTH_CRITICAL(_config.stage.module == VK_NULL_HANDLE, "the stage needs a shader module") {}

    auto const mapped = mapStages(std::span{&_config.stage, 1}, _config.descriptorMappings);
    auto const flags = createFlags(_config.flags, nullptr);

    VkComputePipelineCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = &flags,
        .flags = 0,
        .stage = mapped.stages[0],
        .layout = VK_NULL_HANDLE,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    BasePipeline::create(info);
}
}
