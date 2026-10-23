#pragma once
#include <volk.h>
#include <glm/glm.hpp>

namespace jly {

inline glm::uvec2 to_uvec2(VkExtent2D const& extent) {
    return {extent.width, extent.height};
}

inline VkExtent2D to_vk_extent(glm::uvec2 const& extent) {
    return {extent.x, extent.y};
}
}