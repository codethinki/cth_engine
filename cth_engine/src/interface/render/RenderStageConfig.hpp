#pragma once
#include "src/vulkan/utility/cth_constants.hpp"

namespace cth::vk {
class Queue;
struct PipelineWaitStage;
class Semaphore;

enum RenderStageFlag : size_t {
    RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING = 1 << 0,
    RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING = 1 << 1
};
using RenderStageFlags = std::underlying_type_t<RenderStageFlag>;
struct RenderSubStageConfig {};

struct RenderStageConfig {
    static constexpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;

    cth::not_null<Queue const*> queue;
    uint32_t subStages = 0;

    std::vector<Semaphore*> signalSemaphores{};
    std::vector<PipelineWaitStage> waitStages{};
    RenderStageFlags flags = RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING;

    [[nodiscard]] cxpr bool parallelFrameInFlightRecording() const { return flags & RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING; }
    [[nodiscard]] cxpr bool parallelSubStageRecording() const { return flags & RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING; }
};
}
