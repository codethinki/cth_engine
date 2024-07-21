#pragma once
#include <vulkan/vulkan.h>



namespace cth {

namespace vk {
    class PrimaryCmdBuffer;
} //namespace vk

struct FrameInfo {
    FrameInfo(uint32_t const frame_index, float const frame_time, vk::PrimaryCmdBuffer const* cmd_buffer) : frameIndex(frame_index),
        frameTime(frame_time),
        commandBuffer(cmd_buffer) {}

    VkPipelineLayout pipelineLayout{}; // set by render system

    //set at creation
    uint32_t frameIndex;
    float frameTime;
    vk::PrimaryCmdBuffer const* commandBuffer;
};

} // namespace cth
