#pragma once
#include "../user/HlcCamera.hpp"

#include <vulkan/vulkan.h>

namespace cth {
struct FrameInfo {
    FrameInfo(const uint_fast8_t frame_index, const float frame_time, VkCommandBuffer command_buffer, Camera& camera) : frameIndex(frame_index),
        frameTime(frame_time),
        commandBuffer(command_buffer),
        camera(camera) {}

    VkPipelineLayout pipelineLayout{}; // set by render system

    //set at creation
    uint_fast8_t frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera& camera;
};
}
