#pragma once
#include "jvk/sync/resource_tracker.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cstdint>


namespace jvk::dev {
/** waits for the release wait of the submit */
constexpr cvk::BarrierScope ACQUIRE_SRC{.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};

[[nodiscard]] constexpr cvk::ImageBarrierInfo release_half(
    ImageState const& last,
    ImageState const& first,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family,
    uint32_t mipLevel
) {
    return cvk::ImageBarrierInfo{
        .src = last.scope,
        .srcQueueFamilyIndex = lastFamily,
        .dstQueueFamilyIndex = family,
        .newLayout = first.layout,
        .oldLayout = last.layout,
        .firstMipLevel = mipLevel,
        .mipLevels = 1,
    };
}

/** the layouts of `release_half()`: both halves of a transfer have the same layouts */
[[nodiscard]] constexpr cvk::ImageBarrierInfo acquire_half(
    ImageState const& last,
    ImageState const& first,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family,
    uint32_t mipLevel
) {
    return cvk::ImageBarrierInfo{
        .src = ACQUIRE_SRC,
        .dst = first.scope,
        .srcQueueFamilyIndex = lastFamily,
        .dstQueueFamilyIndex = family,
        .newLayout = first.layout,
        .oldLayout = last.layout,
        .firstMipLevel = mipLevel,
        .mipLevels = 1,
    };
}

[[nodiscard]] constexpr cvk::BufferBarrierInfo release_half(
    BufferState const& last,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family
) {
    return cvk::BufferBarrierInfo{
        .src = last.scope,
        .srcQueueFamilyIndex = lastFamily,
        .dstQueueFamilyIndex = family,
    };
}

[[nodiscard]] constexpr cvk::BufferBarrierInfo acquire_half(
    BufferState const& first,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family
) {
    return cvk::BufferBarrierInfo{
        .src = ACQUIRE_SRC,
        .dst = first.scope,
        .srcQueueFamilyIndex = lastFamily,
        .dstQueueFamilyIndex = family,
    };
}
}
