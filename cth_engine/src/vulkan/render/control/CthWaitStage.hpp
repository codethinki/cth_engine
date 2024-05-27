#pragma once
#include <vulkan/vulkan.h>

namespace cth {
class BasicSemaphore;


struct PipelineWaitStage {
    VkPipelineStageFlags stage;
    const BasicSemaphore* semaphore;
};

} //namespace cth
