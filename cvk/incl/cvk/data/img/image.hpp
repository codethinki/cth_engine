#pragma once
#include "image_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/utility/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <bit>
#include <optional>
#include <span>
#include <vector>


namespace cvk {
class Core;

/**
 * @brief VkImage wrapper, owns the image and its memory, tracks the layout of each mip level
 * @details @ref create() creates both with vmaCreateImage(),
 *  swapchain images are wrapped without memory
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
     * @pre @ref State::levelLayouts size <= @ref Config::mipLevels
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the image and its memory
     * @post @ref created()
     * @post every mip level has @ref Config::initialLayout
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
    [[nodiscard]] constexpr VkImageAspectFlags aspectMask() const { return _config.aspectMask; }
    [[nodiscard]] constexpr uint32_t mipLevels() const { return _config.mipLevels; }

    /**
     * @pre mipLevel < @ref mipLevels()
     */
    [[nodiscard]] constexpr VkImageLayout layout(uint32_t mipLevel) const { return _levelLayouts[mipLevel]; }
    [[nodiscard]] constexpr std::span<VkImageLayout const> layouts() const { return _levelLayouts; }

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
    std::vector<VkImageLayout> _levelLayouts;

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
    /**
     * @brief current layout of each mip level
     * @details size < @ref Config::mipLevels -> the other levels have @ref Config::initialLayout
     */
    std::vector<VkImageLayout> levelLayouts{};
};
}

namespace cvk {
constexpr void Image::debug_check(Image const& image) {
    CTH_CRITICAL(!image.created(), "image must be created") {}
}
}
