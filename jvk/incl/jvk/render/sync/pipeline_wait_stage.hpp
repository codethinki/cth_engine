#pragma once
#include <volk.h>
#include <cth/io/log.hpp>


namespace jvk {
class Semaphore;


struct PipelineWaitStage {
    using stage_t = VkPipelineStageFlags;

    VkPipelineStageFlags stage;
    Semaphore const* semaphore;

    static void debug_check(PipelineWaitStage wait_stage);
};

}

//debug checks

namespace jvk {
inline void PipelineWaitStage::debug_check(PipelineWaitStage wait_stage) {
    CTH_CRITICAL(wait_stage.stage != 0, "stage must not be 0") {}
    CTH_CRITICAL(wait_stage.semaphore == nullptr, "semaphore must not be nullptr") {}
}
}