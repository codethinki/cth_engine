#pragma once

#include "jolly/render/submit/queue_properties.hpp"

#include "jvk/base/queue/queue_family.hpp"


namespace jly {
[[nodiscard]] constexpr jvk::QueueFamilyProperties to_queue_family_properties(jly::QueueProperties properties) {
    jvk::QueueFamilyProperties out{};

    if(contains(properties, QueueProperties::COMPUTE)) out |= jvk::QueueFamilyProperties::COMPUTE;
    if(contains(properties, QueueProperties::TRANSFER)) out |= jvk::QueueFamilyProperties::TRANSFER;
    if(contains(properties, QueueProperties::GRAPHICS)) out |= jvk::QueueFamilyProperties::GRAPHICS;
    if(contains(properties, QueueProperties::PRESENT)) out |= jvk::QueueFamilyProperties::PRESENT;

    return out;
}
}
