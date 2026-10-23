#pragma once
#include <volk.h>



namespace jvk {
class PrimaryCmdBuffer;
}

namespace cth {

struct FrameInfo {
    size_t frameIndex;
    float frameTime;
    jvk::PrimaryCmdBuffer const* commandBuffer;
};

} // namespace cth
