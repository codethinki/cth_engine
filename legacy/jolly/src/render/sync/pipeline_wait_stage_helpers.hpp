#pragma once
#include "pipeline_stage_flags_helpers.hpp"
#include "jolly/render/sync/pipeline_wait_stage.hpp"

#include <jvk/render/sync/pipeline_wait_stage.hpp>


namespace jly {
constexpr jvk::PipelineWaitStage to_vk(PipelineWaitStage const& stage) {
    return {to_vk(stage.stage), stage.semaphore};
}
}
