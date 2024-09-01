#include "CthWaitStage.hpp"

#include "CthSemaphore.hpp"

namespace cth::vk {


void PipelineWaitStage::debug_check(PipelineWaitStage wait_stage) {
    DEBUG_CHECK_SEMAPHORE(wait_stage.semaphore);
}

}
