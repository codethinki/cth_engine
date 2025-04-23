module;
#include <cth/macro.hpp>

export module cth.interface.render.stage_config;

import cth.vk.constants;

import cth.ptr.not_null;

import std;

namespace cth::vk {
class Queue;
struct PipelineWaitStage;
class Semaphore;
}

export namespace cth::vk {
enum RenderStageFlag : size_t {
    RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING = 1 << 0,
    RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING = 1 << 1
};
using RenderStageFlags = std::underlying_type_t<RenderStageFlag>;
struct RenderSubStageConfig {};

struct RenderStageConfig {
    static cxpr auto GROUP_SIZE = constants::FRAMES_IN_FLIGHT;

    cth::not_null<Queue const*> queue;
    uint32_t subStages = 0;

    std::vector<Semaphore*> signalSemaphores{};
    std::vector<PipelineWaitStage> waitStages{};
    RenderStageFlags flags = RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING;

    [[nodiscard]] cxpr bool parallelFrameInFlightRecording() const { return flags & RENDER_STAGE_PARALLEL_FRAMES_IN_FLIGHT_RECORDING; }
    [[nodiscard]] cxpr bool parallelSubStageRecording() const { return flags & RENDER_STAGE_PARALLEL_SUB_STAGE_RECORDING; }
};
}
