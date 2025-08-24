#pragma once


#include "jvk/utility/constants.hpp"

#include <cth/pointer/not_null.hpp>


namespace jvk {
class Queue;
struct PipelineWaitStage;
class Semaphore;
}

namespace jly {
enum RenderStageFlag : size_t {
    RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING = 1 << 0,
    RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING = 1 << 1
};

using RenderStageFlags = std::underlying_type_t<RenderStageFlag>;

struct RenderSubStageConfig {};

struct RenderStageConfig {
    static constexpr auto GROUP_SIZE = jvk::constants::FRAMES_IN_FLIGHT;

    cth::not_null<jvk::Queue const*> queue;
    uint32_t subStages = 0;

    std::vector<jvk::Semaphore*> signalSemaphores{};
    std::vector<jvk::PipelineWaitStage> waitStages{};
    RenderStageFlags flags = RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING;

    [[nodiscard]] cxpr bool parallelFrameInFlightRecording() const {
        return flags & RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING;
    }

    [[nodiscard]] cxpr bool parallelSubStageRecording() const {
        return flags & RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING;
    }
};
}
