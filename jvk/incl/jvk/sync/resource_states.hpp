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
     * stages, access and owning queue family of the access
     * @details VK_QUEUE_FAMILY_IGNORED -> the family of the recording
     */
    cvk::BarrierScope scope;
};

/**
 * access of a command to the mip levels of an image, every layer
 */
struct ImageAccess {
    /**
     * layout of the mip levels during the access
     */
    VkImageLayout layout;

    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;

    uint32_t firstMipLevel = 0;

    /**
     * (VK_REMAINING_MIP_LEVELS => to the last mip level)
     */
    uint32_t mipLevels = VK_REMAINING_MIP_LEVELS;
};

/**
 * state of a whole buffer after an access
 */
struct BufferState {
    /**
     * stages, access and owning queue family of the access
     * @details VK_QUEUE_FAMILY_IGNORED -> the family of the recording
     */
    cvk::BarrierScope scope;
};

/**
 * access of a command to a buffer, counts for the whole buffer
 */
struct BufferAccess {
    VkPipelineStageFlags2 stages;
    VkAccessFlags2 access;
};

/**
 * first and last state of the mip levels and the buffers in one recording, the barriers between the accesses
 * @details the caller owns it and decides the scope, a jvk command buffer has one per recording,
 *  images: key (image, mip level), all layers of a mip level share one state,
 *  buffers: one state per buffer, no ranges,
 *  the first state spans the accesses until the first barrier,
 *  the submit resolves the first states against the states before the recording,
 *  @ref assume() sets a state without a barrier, for barriers of the caller and external Vulkan code
 * @attention follows the order of the commands, use it on one thread
 * @attention keeps the entry of a destroyed image or buffer and a new one can get the same handle,
 *  call @ref erase() before it is destroyed
 * @attention keeps a pointer to the image or buffer of the last access for the barriers of @ref append(),
 *  the images and buffers do not move while the states hold them
 */
class ResourceStates {
public:
    /**
     * records the access of the mip levels
     * @details per mip level:
     *  - no state -> notes the access as its first use, no barrier
     *  - layout change, or the last state or the access writes -> appends a barrier from the last state to the access,
     *    the access becomes the last state
     *  - read after read in the same layout -> no barrier, the last state gets the stages and the access too
     * @param barrier gets the barriers, the caller records it before the command of the access
     * @pre @ref cvk::Image::created()
     * @pre the mip range lies in the image
     */
    void access(cvk::Image const&, ImageAccess const&, cvk::Barrier& barrier);

    /**
     * records the access of the buffer
     * @details the rules of @ref access(cvk::Image const&, ImageAccess const&, cvk::Barrier&) without the layout:
     *  - no state -> notes the access as its first use, no barrier
     *  - the last state or the access writes -> appends a barrier from the last state to the access,
     *    the access becomes the last state
     *  - read after read -> no barrier, the last state gets the stages and the access too
     * @param barrier gets the barrier, the caller records it before the command of the access
     * @pre @ref cvk::BaseBuffer::created()
     */
    void access(cvk::BaseBuffer const&, BufferAccess const&, cvk::Barrier& barrier);

    /**
     * sets the last state of the mip levels, records no barrier
     * @details for barriers that the caller recorded and for external Vulkan code,
     *  mip level without a state -> no first use, the submit resolves nothing for it: the caller synchronized it,
     *  mip level with a state -> keeps its first use, the accesses after the assume do not extend it
     * @param state of the last access of that code, unknown -> VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT with
     *  VK_ACCESS_2_MEMORY_WRITE_BIT
     * @param mipLevels (VK_REMAINING_MIP_LEVELS => to the last mip level)
     * @pre @ref cvk::Image::created()
     * @pre the mip range lies in the image
     */
    void assume(
        cvk::Image const&,
        ImageState const& state,
        uint32_t firstMipLevel = 0,
        uint32_t mipLevels = VK_REMAINING_MIP_LEVELS
    );

    /**
     * sets the last state of the buffer, records no barrier
     * @details for barriers that the caller recorded and for external Vulkan code,
     *  buffer without a state -> no first use, the submit resolves nothing for it: the caller synchronized it,
     *  buffer with a state -> keeps its first use, the accesses after the assume do not extend it
     * @param state of the last access of that code, unknown -> VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT with
     *  VK_ACCESS_2_MEMORY_WRITE_BIT
     * @pre @ref cvk::BaseBuffer::created()
     */
    void assume(cvk::BaseBuffer const&, BufferState const& state);

    /**
     * appends the accesses of a later recording, e.g. a secondary that a primary executes
     * @details the mip levels and buffers of next follow the rules of @ref access():
     *  barriers for the first states of next that follow a last state of this,
     *  the first states of next without a state in this become first states,
     *  an entry of next without a first use (@ref assume()) gets no barrier,
     *  the last states of next become the last states
     * @param next states of the later recording
     * @param barrier gets the barriers, the caller records it before the commands of next
     */
    void append(ResourceStates const& next, cvk::Barrier& barrier);

    /**
     * @return state of the first use of the mip level in the recording,
     *  std::nullopt -> no access, or an @ref assume() before the first access
     */
    [[nodiscard]] std::optional<ImageState> first(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return state after the last access or @ref assume() of the mip level, std::nullopt -> none
     */
    [[nodiscard]] std::optional<ImageState> last(cvk::Image const&, uint32_t mipLevel) const;

    /**
     * @return state of the first use of the buffer in the recording,
     *  std::nullopt -> no access, or an @ref assume() before the first access
     */
    [[nodiscard]] std::optional<BufferState> first(cvk::BaseBuffer const&) const;

    /**
     * @return state after the last access or @ref assume() of the buffer, std::nullopt -> none
     */
    [[nodiscard]] std::optional<BufferState> last(cvk::BaseBuffer const&) const;

    /**
     * removes every mip level of the image, they have no state after
     */
    void erase(cvk::Image const&);

    /**
     * removes the buffer, it has no state after
     */
    void erase(cvk::BaseBuffer const&);

    /**
     * removes every image and buffer
     */
    void clear() {
        _images.clear();
        _buffers.clear();
    }

    /**
     * @return a mip level of the image has a state
     */
    [[nodiscard]] bool contains(cvk::Image const&) const;

    /**
     * @return the buffer has a state
     */
    [[nodiscard]] bool contains(cvk::BaseBuffer const&) const;

    /**
     * @return number of mip levels and buffers with a state
     */
    [[nodiscard]] size_t size() const { return _images.size() + _buffers.size(); }

    [[nodiscard]] bool empty() const { return _images.empty() && _buffers.empty(); }

private:
    using image_key_t = std::pair<VkImage, uint32_t>;

    /**
     * states of one mip level
     */
    struct image_states {
        /**
         * image of the last access or assume, for the barriers of @ref append()
         */
        cth::not_null_ptr<cvk::Image const> image;

        /**
         * std::nullopt -> an @ref assume() came before the first access
         */
        std::optional<ImageState> first;
        ImageState last;

        /**
         * a barrier or an @ref assume() followed the first use, the accesses after it do not extend @ref first
         */
        bool barrier = false;
    };

    /**
     * states of one buffer
     */
    struct buffer_states {
        /**
         * buffer of the last access or assume, for the barriers of @ref append()
         */
        cth::not_null_ptr<cvk::BaseBuffer const> buffer;

        /**
         * std::nullopt -> an @ref assume() came before the first access
         */
        std::optional<BufferState> first;
        BufferState last;

        /**
         * a barrier or an @ref assume() followed the first use, the accesses after it do not extend @ref first
         */
        bool barrier = false;
    };

    cth::dt::flat_hash_map<image_key_t, image_states> _images{};
    cth::dt::flat_hash_map<VkBuffer, buffer_states> _buffers{};
};
}
