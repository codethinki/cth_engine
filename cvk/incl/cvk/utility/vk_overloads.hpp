#pragma once
#include <volk.h>

constexpr bool operator==(VkSurfaceFormatKHR const& a, VkSurfaceFormatKHR const& b) {
    return a.format == b.format && a.colorSpace == b.colorSpace;
}

constexpr bool operator==(VkExtent2D const& a, VkExtent2D const& b) {
    return a.width == b.width && a.height == b.height;
}

constexpr bool operator==(VkExtent3D const& a, VkExtent3D const& b) {
    return a.width == b.width && a.height == b.height && a.depth == b.depth;
}

constexpr bool operator==(VkQueueFamilyProperties const& a, VkQueueFamilyProperties const& b) {
    return a.queueFlags == b.queueFlags
        && a.queueCount == b.queueCount
        && a.timestampValidBits == b.timestampValidBits
        && a.minImageTransferGranularity == b.minImageTransferGranularity;
}
