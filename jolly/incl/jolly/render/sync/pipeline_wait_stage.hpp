#pragma once
#include "jolly/render/sync/pipeline_stage_flags.hpp"

namespace jvk {
class Semaphore;
}

namespace jly {


struct PipelineWaitStage {
    using stage_t = PipelineStageFlags;

    stage_t stage;
    jvk::Semaphore const* semaphore;
};



}