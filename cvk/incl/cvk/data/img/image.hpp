#pragma once
#include "image_config.hpp"
#include "image_format.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <bit>
#include <optional>


namespace cvk {
class BaseBuffer;
class BaseCmdBuffer;
class Core;

/**
 * @brief VkImage wrapper, owns the image and its memory
 * @details @ref create() creates both with vmaCreateImage(),
 *  swapchain images are wrapped without memory,
 *  the image holds no layout, the commands take it as a value
 */
class Image {
public:
    using vk_handle_t = VkImage;
    using Config = ImageConfig;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    Image(Core const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Image(Core const&, Config const&)
     * - @ref wrap()
     */
    Image(Core const&, Config const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Image(Core const&, Config const&)
     * - @ref create()
     */
    Image(Core const&, Config const&, VkExtent2D extent);

    /**
     * @note calls @ref optDestroy()
     */
    ~Image() { optDestroy(); }

    /**
     * @brief wraps the state
     * @details std::nullopt @ref State::memory -> @ref destroy() only destroys the image
     * @pre the image is bound to its memory
     * @pre the image was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the image and its memory
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vmaCreateImage()
     */
    void create(VkExtent2D extent);

    /**
     * @brief destroys the image, then its memory, and forgets
     * @pre @ref created()
     * @pre the gpu does not use the image
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership of the image and its memory, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief records a copy from the buffer into the mip level
     * @param srcOffset in bytes
     * @param layout of the mip level, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL or VK_IMAGE_LAYOUT_GENERAL
     * @pre @ref created() and src created
     * @pre @ref BaseCmdBuffer::recording()
     * @pre mipLevel < @ref mipLevels()
     * @pre @ref aspectMask() has one bit
     * @pre src usage has VK_BUFFER_USAGE_TRANSFER_SRC_BIT, the usage has VK_IMAGE_USAGE_TRANSFER_DST_BIT
     */
    void copy(
        BaseCmdBuffer const&,
        BaseBuffer const& src,
        size_t srcOffset = 0,
        uint32_t mipLevel = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    ) const;

    /**
     * @return mip levels down to 1x1
     * @pre width > 0, height > 0
     */
    [[nodiscard]] static constexpr uint32_t evalMipLevelCount(VkExtent2D extent) {
        return static_cast<uint32_t>(std::bit_width(std::max(extent.width, extent.height)));
    }

    static void destroy(DeviceTable table, VkImage);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    [[nodiscard]] constexpr VkExtent2D extent() const { return _extent; }
    [[nodiscard]] constexpr VkFormat format() const { return _config.format; }
    /**
     * @return @ref Config::aspectMask, 0 -> the aspects of the format
     */
    [[nodiscard]] constexpr VkImageAspectFlags aspectMask() const {
        return _config.aspectMask != 0 ? _config.aspectMask : format_aspects(_config.format);
    }
    [[nodiscard]] constexpr uint32_t mipLevels() const { return _config.mipLevels; }

    /**
     * @return the memory of the image, not created -> the image does not own its memory
     */
    [[nodiscard]] constexpr Memory const& memory() const { return _memory; }

private:
    void forget();

    // declared before the memory: move assignment destroys the old image before its memory
    core_handle<Image> _handle;
    Memory _memory;
    Config _config;
    VkExtent2D _extent{};

public:
    Image(Image const& other) = delete;
    Image(Image&& other) noexcept = default;
    Image& operator=(Image const& other) = delete;
    Image& operator=(Image&& other) noexcept = default;

    static constexpr void debug_check(Image const&);
};
}

namespace cvk {
struct Image::State {
    vk_not_null<VkImage> vkImage;
    VkExtent2D extent;
    /**
     * @brief std::nullopt -> the image does not own its memory, e.g. swapchain images
     */
    std::optional<Memory::State> memory = std::nullopt;
};
}

namespace cvk {
constexpr void Image::debug_check(Image const& image) {
    CTH_CRITICAL(!image.created(), "image must be created") {}
}
}
