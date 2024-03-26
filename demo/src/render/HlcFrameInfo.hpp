#pragma once
#include <vulkan/vulkan.h>

namespace cth {
struct FrameInfo {
    FrameInfo(const uint32_t frame_index, const float frame_time, VkCommandBuffer command_buffer) : frameIndex(frame_index),
        frameTime(frame_time),
        commandBuffer(command_buffer) {}

    VkPipelineLayout pipelineLayout{}; // set by render system

    //set at creation
    uint32_t frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
};

} // namespace cth
