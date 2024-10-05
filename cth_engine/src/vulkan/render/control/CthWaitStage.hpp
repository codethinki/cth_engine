#pragma once
#include <vulkan/vulkan.h>

#include "vulkan/utility/cth_constants.hpp"

namespace cth::vk {
class Semaphore;


struct PipelineWaitStage {
    VkPipelineStageFlags stage;
    Semaphore const* semaphore;

    static void debug_check(PipelineWaitStage wait_stage);
};

}

//debug checks

namespace cth::vk {
inline void PipelineWaitStage::debug_check(PipelineWaitStage wait_stage) {
    CTH_CRITICAL(wait_stage.stage != 0, "stage must not be 0") {}
    CTH_CRITICAL(wait_stage.semaphore == nullptr, "semaphore must not be nullptr") {}
}
}
