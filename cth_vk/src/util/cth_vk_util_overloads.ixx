module;
#include "lib/volk.hpp"

export module cth.vk.util.overloads;

inline bool operator==(VkSurfaceFormatKHR const& lhs, VkSurfaceFormatKHR const& rhs) {
    return lhs.format == rhs.format && lhs.colorSpace == rhs.colorSpace;
}