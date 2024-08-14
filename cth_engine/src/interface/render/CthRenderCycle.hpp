#pragma once
#include "vulkan/utility/cth_constants.hpp"

namespace cth::vk {
struct Cycle {
    size_t index;
    size_t subIndex;

    static Cycle Next(Cycle const& old);
};
} //namespace cth::vk



namespace cth::vk {
inline Cycle Cycle::Next(Cycle const& old) {
    return Cycle{
        .index = old.index + 1,
        .subIndex = (old.subIndex + 1) % constants::FRAMES_IN_FLIGHT,
    };
}
}
