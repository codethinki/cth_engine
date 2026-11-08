#pragma once
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/data/hash_map.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>


namespace cvk {
class BaseBuffer;
class Image;
}

namespace jvk {
/**
 * state of one mip level after an access
 */
struct ImageState {
    VkImageLayout layout;

    cvk::BarrierScope scope; ///< `VK_QUEUE_FAMILY_IGNORED` -> the family of the recording
};

/**
 * image access of a command
 * @details all layers and aspects of the mip levels
 */
struct ImageAccess {
    VkImageLayout layout;

    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;

    uint32_t firstMipLevel = 0;

    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS; ///< (`VK_REMAINING_MIP_LEVELS` => to the last mip level)

    //TODO review
    /**
     * the access does not need the previous contents
     * @attention all layers and aspects of the mip levels lose their contents
     */
    bool discard = false;
};

/**
 * state of a buffer after an access
 */
struct BufferState {
    cvk::BarrierScope scope; ///< `VK_QUEUE_FAMILY_IGNORED` -> the family of the recording
};

/**
 * buffer access of a command
 * @details the whole buffer
 */
struct BufferAccess {
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
};

/**
 * resource state tracker of one recording
 * @details one state per (image, mip level), for all layers and aspects
 * @details one state per buffer
 * @attention not thread safe
 * @attention keeps pointers: the images and buffers do not move
 * @attention call `erase()` before an image or buffer is destroyed
 */
class ResourceStates {
public:
    /**
     * records the access of the mip levels
     * @details no state -> first use without a barrier
     * @param[out] barrier gets the barriers, record it before the command of the access
     * @pre the mip range lies in the image
     */
    void access(cvk::Image const&, ImageAccess const&, cvk::Barrier& barrier);

    /**
     * records the access of the buffer
     * @details no state -> first use without a barrier
     * @param[out] barrier gets the barrier, record it before the command of the access
     */
    void access(cvk::BaseBuffer const&, BufferAccess const&, cvk::Barrier& barrier);

    /**
     * sets the last state of the mip levels after untracked code, without a barrier
     * @details no state -> no first use
     * @param state unknown -> `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`, `VK_ACCESS_2_MEMORY_WRITE_BIT`
     * @pre the mip range lies in the image
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the last state of the buffer after untracked code, without a barrier
     * @details no state -> no first use
     * @param state unknown -> `VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT`, `VK_ACCESS_2_MEMORY_WRITE_BIT`
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * adds the stages and the access to the last state of the mip levels, without a barrier
     * @details no state -> stays without a state
     * @details no barrier since the first use -> adds them to the first state too
     * @pre the mip range lies in the image
     */
    void extend(
        cvk::Image const&,
        cvk::BarrierScope const& scope,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * appends the states of a later recording
     * @param[out] barrier gets the barriers, record it before the commands of next
     */
    void append(ResourceStates const& next, cvk::Barrier& barrier);

    /**
     * @return `std::nullopt` -> no access or an `assume()` before it
     */
    [[nodiscard]] std::optional<ImageState> first(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return the first use of the mip level has `ImageAccess::discard`
     */
    [[nodiscard]] bool discards(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return `std::nullopt` -> no access and no `assume()`
     */
    [[nodiscard]] std::optional<ImageState> last(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return `std::nullopt` -> no access or an `assume()` before it
     */
    [[nodiscard]] std::optional<BufferState> first(cvk::BaseBuffer const&) const;

    /**
     * @return `std::nullopt` -> no access and no `assume()`
     */
    [[nodiscard]] std::optional<BufferState> last(cvk::BaseBuffer const&) const;

    void erase(cvk::Image const&);
    void erase(cvk::BaseBuffer const&);

    void clear() {
        _images.clear();
        _buffers.clear();
    }

    /**
     * @return a mip level of the image has a state
     */
    [[nodiscard]] bool contains(cvk::Image const&) const;
    [[nodiscard]] bool contains(cvk::BaseBuffer const&) const;

    /**
     * @return number of mip levels and buffers with a state
     */
    [[nodiscard]] size_t size() const { return _images.size() + _buffers.size(); }

    [[nodiscard]] bool empty() const { return _images.empty() && _buffers.empty(); }

private:
    using image_key_t = std::pair<VkImage, uint32_t>;

    struct image_states {
        cth::ref_wrapper<cvk::Image const> image;

        std::optional<ImageState> firstOpt; ///< `std::nullopt` -> `assume()` before the first access
        ImageState last;
        bool discard = false;

        bool barrier = false; ///< a barrier or `assume()` after the first use: `firstOpt` is final
    };

    struct buffer_states {
        cth::ref_wrapper<cvk::BaseBuffer const> buffer;

        std::optional<BufferState> firstOpt; ///< `std::nullopt` -> `assume()` before the first access
        BufferState last;

        bool barrier = false; ///< a barrier or `assume()` after the first use: `firstOpt` is final
    };

    cth::dt::flat_hash_map<image_key_t, image_states> _images{};
    cth::dt::flat_hash_map<VkBuffer, buffer_states> _buffers{};
};
}
