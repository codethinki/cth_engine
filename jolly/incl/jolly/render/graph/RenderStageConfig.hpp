#pragma once
#include "jolly/render/sync/pipeline_wait_stage.hpp"

#include <cth/ptr/not_null.hpp>

namespace jvk {
class Semaphore;
}

namespace jly {
struct PipelineWaitStage;
class Queue;

enum class RenderStageFlags : size_t {
    NONE,
    PARALLEL_SUB_STAGE_RECORDING = 1 << 0,
    PARALLEL_FRAMES_IN_FLIGHT_RECORDING = 1 << 1
};

}


CTH_GEN_ENUM_FLAG_OVERLOADS(jly::RenderStageFlags)

namespace jly {


struct RenderSubStageConfig {};

struct RenderStageConfig {
    cth::not_null<Queue const*> queue;
    uint32_t subStages = 0;

    std::vector<jvk::Semaphore const*> signalSemaphores{};
    std::vector<PipelineWaitStage> waitStages{};
    RenderStageFlags flags = RenderStageFlags::PARALLEL_SUB_STAGE_RECORDING;

    [[nodiscard]] cxpr bool parallelFrameInFlightRecording() const {
        return contains(flags, RenderStageFlags::PARALLEL_FRAMES_IN_FLIGHT_RECORDING);
    }

    [[nodiscard]] cxpr bool parallelSubStageRecording() const {
        return contains(flags, RenderStageFlags::PARALLEL_SUB_STAGE_RECORDING);
    }
};
}
