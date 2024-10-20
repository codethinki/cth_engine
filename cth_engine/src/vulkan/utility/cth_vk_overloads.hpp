#pragma once
#include <volk.h>

inline bool operator==(VkSurfaceFormatKHR const& lhs, VkSurfaceFormatKHR const& rhs) {
    return lhs.format == rhs.format && lhs.colorSpace == rhs.colorSpace;
}