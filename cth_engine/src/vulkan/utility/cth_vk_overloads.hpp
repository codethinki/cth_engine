#pragma once
#include <volk.h>

cxpr bool operator==(VkSurfaceFormatKHR const& a, VkSurfaceFormatKHR const& b) {
    return a.format == b.format && a.colorSpace == b.colorSpace;
}
cxpr bool operator==(VkExtent2D const& a, VkExtent2D const& b) {
    return a.width == b.width && a.height == b.height;
}

