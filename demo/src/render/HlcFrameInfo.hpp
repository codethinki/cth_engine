#pragma once
#include <vulkan/vulkan.h>



namespace cth {

namespace vk {
    class PrimaryCmdBuffer;
} //namespace vk

struct FrameInfo {
    FrameInfo(const uint32_t frame_index, const float frame_time, const vk::PrimaryCmdBuffer* cmd_buffer) : frameIndex(frame_index),
        frameTime(frame_time),
        commandBuffer(cmd_buffer) {}

    VkPipelineLayout pipelineLayout{}; // set by render system

    //set at creation
    uint32_t frameIndex;
    float frameTime;
    const vk::PrimaryCmdBuffer* commandBuffer;
};

} // namespace cth
