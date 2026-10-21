#pragma once
#include "cvk/utility/vk_hash.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <volk.h>

#include <cth/enums.hpp>


namespace cvk {
enum class QueueFamilyProperties {
    NONE = 0,
    GRAPHICS = cth::en::flag_val(0),
    COMPUTE = cth::en::flag_val(1),
    TRANSFER = cth::en::flag_val(2),
    PRESENT = cth::en::flag_val(3)
};
}

CTH_GEN_ENUM_FLAG_OVERLOADS(cvk::QueueFamilyProperties)

namespace cvk {
/**
 * @param presentSupport the family presents to the surfaces
 * @return the properties of a queue family with the flags
 * @details graphics or compute -> transfer, also without VK_QUEUE_TRANSFER_BIT:
 *  Vulkan lets graphics and compute families leave the bit out
 */
constexpr QueueFamilyProperties to_queue_properties(VkQueueFlags flags, bool presentSupport) {
    constexpr VkQueueFlags transfer = VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    auto result = QueueFamilyProperties::NONE;
    if(flags & VK_QUEUE_GRAPHICS_BIT) result |= QueueFamilyProperties::GRAPHICS;
    if(flags & VK_QUEUE_COMPUTE_BIT) result |= QueueFamilyProperties::COMPUTE;
    if(flags & transfer) result |= QueueFamilyProperties::TRANSFER;
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

    [[nodiscard]] constexpr bool operator==(QueueFamily const&) const = default;
};
}

CTH_HASH_AGGREGATE(cvk::QueueFamily)
