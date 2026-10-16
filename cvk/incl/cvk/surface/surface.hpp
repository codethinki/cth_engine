#pragma once
#include "surface_config.hpp"

#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <vector>


namespace cvk {
class Instance;
class PhysicalDevice;

/**
 * @brief VkSurfaceKHR wrapper
 * @details the window system creates the surface, @ref wrap() takes it
 */
class Surface {
public:
    using Config = SurfaceConfig;
    using format_t = Config::format_t;
    using present_mode_t = Config::present_mode_t;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Instance::created()
     */
    explicit Surface(Instance const&, Config = {});

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Surface(Instance const&, Config)
     * - @ref wrap()
     */
    Surface(Instance const&, Config, State const&);

    /**
     * @note calls @ref optDestroy()
     */
    ~Surface() { optDestroy(); }

    /**
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @return queue family @ref familyIndex can present to this surface
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceSupportKHR()
     */
    [[nodiscard]] bool supportsFamily(PhysicalDevice const&, uint32_t familyIndex) const;

    /**
     * @return allowed and available present modes, in the order of @ref Config::allowedPresentModes
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfacePresentModesKHR()
     */
    [[nodiscard]] std::vector<present_mode_t> presentModes(PhysicalDevice const&) const;

    /**
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceCapabilitiesKHR()
     */
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const&) const;

    /**
     * @return allowed and available formats, in the order of @ref Config::allowedSurfaceFormats
     * @attention no format -> the surface is not usable with the physical device
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceFormatsKHR()
     */
    [[nodiscard]] std::vector<format_t> formats(PhysicalDevice const&) const;

    /**
     * @return the most preferred allowed and available format
     * @throws cth::except::default_exception reason: no allowed format available
     */
    [[nodiscard]] format_t format(PhysicalDevice const&) const;

    /**
     * @return the most preferred allowed and available present mode
     * @throws cth::except::default_exception reason: no allowed present mode available
     */
    [[nodiscard]] present_mode_t presentMode(PhysicalDevice const&) const;

    static void destroy(vk_not_null<VkInstance>, VkSurfaceKHR);

    [[nodiscard]] bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }
    [[nodiscard]] Config const& config() const { return _config; }

private:
    void forget();

    Config _config;
    unique_handle<VkSurfaceKHR, instance_deleter<Surface>> _handle;

public:
    Surface(Surface const& other) = delete;
    Surface(Surface&& other) noexcept = default;
    Surface& operator=(Surface const& other) = delete;
    Surface& operator=(Surface&& other) noexcept = default;

    static void debug_check(Surface const&);
    static void debug_check_handle(vk_not_null<VkSurfaceKHR>);
};
}

namespace cvk {
struct Surface::State {
    vk_not_null<VkSurfaceKHR> vkSurface;
};
}

namespace cvk {
inline void Surface::debug_check(Surface const& surface) {
    CTH_CRITICAL(!surface.created(), "surface must be created") {}
    debug_check_handle(surface.get());
}
inline void Surface::debug_check_handle([[maybe_unused]] vk_not_null<VkSurfaceKHR> vkSurface) {}
}
