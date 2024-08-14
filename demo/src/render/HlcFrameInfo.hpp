#pragma once
#include <vulkan/vulkan.h>



namespace cth {

namespace vk {
    class PrimaryCmdBuffer;
} //namespace vk

struct FrameInfo {
    size_t frameIndex;
    float frameTime;
    vk::PrimaryCmdBuffer const* commandBuffer;
};

} // namespace cth
