#pragma once
#include <vulkan/vulkan.h>



namespace cth {

class PrimaryCmdBuffer;

struct FrameInfo {
    FrameInfo(const uint32_t frame_index, const float frame_time, const PrimaryCmdBuffer* cmd_buffer) : frameIndex(frame_index),
        frameTime(frame_time),
        commandBuffer(cmd_buffer) {}

    VkPipelineLayout pipelineLayout{}; // set by render system

    //set at creation
    uint32_t frameIndex;
    float frameTime;
    const PrimaryCmdBuffer* commandBuffer;
};

} // namespace cth
