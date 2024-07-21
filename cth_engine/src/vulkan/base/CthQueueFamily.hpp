#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace cth::vk {
enum QueueFamilyPropertyFlagBits : uint32_t {
    QUEUE_FAMILY_PROPERTY_GRAPHICS = VK_QUEUE_GRAPHICS_BIT,
    QUEUE_FAMILY_PROPERTY_COMPUTE = VK_QUEUE_COMPUTE_BIT,
    QUEUE_FAMILY_PROPERTY_TRANSFER = VK_QUEUE_TRANSFER_BIT,
    QUEUE_FAMILY_PROPERTY_PRESENT = 8u,
};

using QueueFamilyProperties = uint32_t;

static QueueFamilyProperties to_queue_properties(VkQueueFlags const flags, bool const present_support) {
    QueueFamilyProperties result{};
    if(flags & VK_QUEUE_GRAPHICS_BIT) result |= QUEUE_FAMILY_PROPERTY_GRAPHICS;
    if(flags & VK_QUEUE_COMPUTE_BIT) result |= QUEUE_FAMILY_PROPERTY_COMPUTE;
    if(flags & VK_QUEUE_TRANSFER_BIT) result |= QUEUE_FAMILY_PROPERTY_TRANSFER;
    if(present_support) result |= QUEUE_FAMILY_PROPERTY_PRESENT;
    return result;
}

struct QueueFamily {


    QueueFamily(uint32_t const index, VkQueueFamilyProperties const& vk_properties, bool const present_support) : index(index),
        properties(to_queue_properties(vk_properties.queueFlags, present_support)), vkProperties(vk_properties) {}

    uint32_t index;
    QueueFamilyProperties properties;
    VkQueueFamilyProperties vkProperties;


};


}
