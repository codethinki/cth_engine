#pragma once
#include <volk.h>

cxpr bool operator==(VkSurfaceFormatKHR const& a, VkSurfaceFormatKHR const& b) {
    return a.format == b.format && a.colorSpace == b.colorSpace;
}

cxpr bool operator==(VkExtent2D const& a, VkExtent2D const& b) {
    return a.width == b.width && a.height == b.height;
}

cxpr bool operator !=(VkExtent2D const& a, VkExtent2D const& b) {
    return !(a == b);
}

cxpr bool operator==(VkExtent3D const& a, VkExtent3D const& b) {
    return a.width == b.width && a.height == b.height && a.depth == b.depth;
}

cxpr bool operator==(VkQueueFamilyProperties const& a, VkQueueFamilyProperties const& b) {
    return a.queueFlags == b.queueFlags
        && a.queueCount == b.queueCount
        && a.timestampValidBits == b.timestampValidBits
        && a.minImageTransferGranularity == b.minImageTransferGranularity;
}


