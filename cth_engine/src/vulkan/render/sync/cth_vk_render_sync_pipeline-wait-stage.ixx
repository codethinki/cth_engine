module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>

export module cth.vk.render.sync.pipeline_wait_stage;


import cth.io.log;

namespace cth::vk {
class Semaphore;
}

export namespace cth::vk {
struct PipelineWaitStage {
    using stage_t = VkPipelineStageFlags;

    VkPipelineStageFlags stage;
    Semaphore const* semaphore;

    static void debug_check(PipelineWaitStage wait_stage);
};

}

//debug checks

export namespace cth::vk {
inline void PipelineWaitStage::debug_check(PipelineWaitStage wait_stage) {
    CTH_CRITICAL(wait_stage.stage != 0, "stage must not be 0") {}
    CTH_CRITICAL(wait_stage.semaphore == nullptr, "semaphore must not be nullptr") {}
}
}
