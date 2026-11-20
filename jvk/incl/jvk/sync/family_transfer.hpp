#pragma once
#include "jvk/sync/resource_states.hpp"

#include "cvk/base/queue/queue_family.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cstdint>


namespace jvk::dev {
/**
 * src of an acquire half: waits for the release wait of the submit, `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`
 */
[[nodiscard]] constexpr cvk::BarrierScope acquire_src(cvk::queue_family_index_t lastFamily) {
    return cvk::BarrierScope{
        .stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .queueFamilyIndex = lastFamily
    };
}

/**
 * release half of the queue family ownership transfer of a mip level, transitions to the first layout
 * @param last state on the last queue
 * @param first state of the first use
 * @param lastFamily of the last queue
 * @param family of the first use
 * @param mipLevel to transfer
 */
[[nodiscard]] constexpr cvk::ImageBarrierInfo release_half(
    ImageState const& last,
    ImageState const& first,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family,
    uint32_t mipLevel
) {
    return cvk::ImageBarrierInfo{
        .src = {last.scope.stages, last.scope.access, lastFamily},
        .dst = {.queueFamilyIndex = family},
        .newLayout = first.layout,
        .oldLayout = last.layout,
        .firstMipLevel = mipLevel,
        .mipLevels = 1,
    };
}

/**
 * acquire half of the queue family ownership transfer of a mip level, the layouts of `release_half()`
 * @param last state on the last queue
 * @param first state of the first use
 * @param lastFamily of the last queue
 * @param family of the first use
 * @param mipLevel to transfer
 */
[[nodiscard]] constexpr cvk::ImageBarrierInfo acquire_half(
    ImageState const& last,
    ImageState const& first,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family,
    uint32_t mipLevel
) {
    return cvk::ImageBarrierInfo{
        .src = acquire_src(lastFamily),
        .dst = {first.scope.stages, first.scope.access, family},
        .newLayout = first.layout,
        .oldLayout = last.layout,
        .firstMipLevel = mipLevel,
        .mipLevels = 1,
    };
}

/**
 * release half of the queue family ownership transfer of a buffer
 * @param last state on the last queue
 * @param lastFamily of the last queue
 * @param family of the first use
 */
[[nodiscard]] constexpr cvk::BufferBarrierInfo release_half(
    BufferState const& last,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family
) {
    return cvk::BufferBarrierInfo::QueueTransition(
        {.queueFamilyIndex = family},
        {last.scope.stages, last.scope.access, lastFamily}
    );
}

/**
 * acquire half of the queue family ownership transfer of a buffer
 * @param first state of the first use
 * @param lastFamily of the last queue
 * @param family of the first use
 */
[[nodiscard]] constexpr cvk::BufferBarrierInfo acquire_half(
    BufferState const& first,
    cvk::queue_family_index_t lastFamily,
    cvk::queue_family_index_t family
) {
    return cvk::BufferBarrierInfo::QueueTransition(
        {first.scope.stages, first.scope.access, family},
        acquire_src(lastFamily)
    );
}
}
