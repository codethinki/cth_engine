#pragma once
#include <vulkan/vulkan.h>

#include "vulkan/utility/CthConstants.hpp"

namespace cth {
class BasicSemaphore;


struct PipelineWaitStage {
    VkPipelineStageFlags stage;
    const BasicSemaphore* semaphore;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(PipelineWaitStage wait_stage);
#define DEBUG_CHECK_PIPELINE_WAIT_STAGE(wait_stage) PipelineWaitStage::debug_check(wait_stage)
#else
#define DEBUG_CHECK_PIPELINE_WAIT_STAGE(wait_stage) ((void)0)
#endif

};

} //namespace cth
