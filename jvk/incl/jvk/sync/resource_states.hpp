#pragma once
#include "cvk/render/sync/barrier.hpp"

#include <volk.h>

#include <cth/data/hash_map.hpp>
#include <cth/ptr/not_null.hpp>

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

    /**
     * VK_QUEUE_FAMILY_IGNORED -> the family of the recording
     */
    cvk::BarrierScope scope;
};

/**
 * access of a command to the mip levels of an image, all layers and aspects
 */
struct ImageAccess {
    VkImageLayout layout;

    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;

    uint32_t firstMipLevel = 0;

    /**
     * (VK_REMAINING_MIP_LEVELS => to the last mip level)
     */
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;

    //TODO review
    /**
     * barrier from VK_IMAGE_LAYOUT_UNDEFINED
     * @attention all layers and aspects of the mip levels lose their contents
     */
    bool discard = false;
};

/**
 * state of a whole buffer after an access
 */
struct BufferState {
    /**
     * VK_QUEUE_FAMILY_IGNORED -> the family of the recording
     */
    cvk::BarrierScope scope;
};

/**
 * access of a command to a whole buffer
 */
struct BufferAccess {
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
};

/**
 * first and last state of the images and buffers in one recording, the barriers between the accesses
 * @details one state per (image, mip level) for all layers and aspects, one per buffer
 * @attention not thread safe, keeps pointers: the images and buffers do not move
 * @attention call @ref erase() before an image or buffer is destroyed
 */
class ResourceStates {
public:
    /**
     * records the access of the mip levels, appends the barriers from their last states
     * @details no state -> first use, read after read in the same layout -> no barrier
     * @param barrier record it before the command of the access
     * @pre the mip range lies in the image
     */
    void access(cvk::Image const&, ImageAccess const&, cvk::Barrier& barrier);

    /**
     * records the access of the buffer, appends the barrier from its last state
     * @details no state -> first use, read after read -> no barrier
     * @param barrier record it before the command of the access
     */
    void access(cvk::BaseBuffer const&, BufferAccess const&, cvk::Barrier& barrier);

    /**
     * sets the last state of the mip levels without a barrier
     * @details no state -> no first use
     * @param state unknown -> VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT
     * @pre the mip range lies in the image
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the last state of the buffer without a barrier
     * @details no state -> no first use
     * @param state unknown -> VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * appends the states of a later recording, appends the barriers for its first uses
     * @param barrier record it before the commands of next
     */
    void append(ResourceStates const& next, cvk::Barrier& barrier);

    /**
     * @return std::nullopt -> no access or an @ref assume() before it
     */
    [[nodiscard]] std::optional<ImageState> first(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return the first use of the mip level has @ref ImageAccess::discard
     */
    [[nodiscard]] bool discards(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return std::nullopt -> no access and no @ref assume()
     */
    [[nodiscard]] std::optional<ImageState> last(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return std::nullopt -> no access or an @ref assume() before it
     */
    [[nodiscard]] std::optional<BufferState> first(cvk::BaseBuffer const&) const;

    /**
     * @return std::nullopt -> no access and no @ref assume()
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
        cth::not_null_ptr<cvk::Image const> image;

        /**
         * std::nullopt -> @ref assume() before the first access
         */
        std::optional<ImageState> first;
        ImageState last;
        bool discard = false;

        /**
         * a barrier or @ref assume() after the first use: @ref first is final
         */
        bool barrier = false;
    };

    struct buffer_states {
        cth::not_null_ptr<cvk::BaseBuffer const> buffer;

        /**
         * std::nullopt -> @ref assume() before the first access
         */
        std::optional<BufferState> first;
        BufferState last;

        /**
         * a barrier or @ref assume() after the first use: @ref first is final
         */
        bool barrier = false;
    };

    cth::dt::flat_hash_map<image_key_t, image_states> _images{};
    cth::dt::flat_hash_map<VkBuffer, buffer_states> _buffers{};
};
}
