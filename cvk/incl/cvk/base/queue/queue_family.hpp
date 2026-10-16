#pragma once
#include "cvk/utility/vk_hash.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <volk.h>

#include <cth/enums.hpp>


namespace cvk {
using cth::en::flag_val;

enum class QueueFamilyProperties {
    NONE = 0,
    GRAPHICS = flag_val(0),
    COMPUTE = flag_val(1),
    TRANSFER = flag_val(2),
    PRESENT = flag_val(3)
};
}

CTH_GEN_ENUM_FLAG_OVERLOADS(cvk::QueueFamilyProperties)

namespace cvk {
constexpr QueueFamilyProperties to_queue_properties(VkQueueFlags flags, bool presentSupport) {
    auto result = QueueFamilyProperties::NONE;
    if(flags & VK_QUEUE_GRAPHICS_BIT) result |= QueueFamilyProperties::GRAPHICS;
    if(flags & VK_QUEUE_COMPUTE_BIT) result |= QueueFamilyProperties::COMPUTE;
    if(flags & VK_QUEUE_TRANSFER_BIT) result |= QueueFamilyProperties::TRANSFER;
    if(presentSupport) result |= QueueFamilyProperties::PRESENT;
    return result;
}

using queue_family_index_t = uint32_t;

struct QueueFamily {
    queue_family_index_t index;
    QueueFamilyProperties properties;
    VkQueueFamilyProperties vkProperties;

    static constexpr QueueFamily Vk(
        queue_family_index_t index,
        VkQueueFamilyProperties const& vkProperties,
        bool presentSupport
    ) {
        return {
            .index = index,
            .properties = to_queue_properties(vkProperties.queueFlags, presentSupport),
            .vkProperties = vkProperties
        };
    }
};

constexpr bool operator==(QueueFamily const& l, QueueFamily const& r) {
    return l.index == r.index && l.properties == r.properties && l.vkProperties == r.vkProperties;
}
}

CTH_HASH_AGGREGATE(cvk::QueueFamily)
