#include "CthWaitStage.hpp"

#include "CthSemaphore.hpp"

namespace cth::vk {


void PipelineWaitStage::debug_check(PipelineWaitStage wait_stage) {
    Semaphore::debug_check(wait_stage.semaphore);
}

}
