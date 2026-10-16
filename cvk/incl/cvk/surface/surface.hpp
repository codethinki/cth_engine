#pragma once
#include "surface_config.hpp"

#include "cvk/utility/types.hpp"

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
    explicit Surface(Instance const& instance, Config config = {});

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Surface(Instance const&, Config)
     * - @ref wrap()
     */
    Surface(Instance const& instance, Config config, State const& state);

    /**
     * @note calls @ref optDestroy()
     */
    ~Surface() { optDestroy(); }

    /**
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief destroys and resets
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and resets
     * @pre @ref created()
     */
    State release();

    /**
     * @return queue family @ref familyIndex can present to this surface
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceSupportKHR()
     */
    [[nodiscard]] bool supportsFamily(PhysicalDevice const& physicalDevice, uint32_t familyIndex) const;

    /**
     * @return allowed and available present modes, in the order of @ref Config::allowedPresentModes
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfacePresentModesKHR()
     */
    [[nodiscard]] std::vector<present_mode_t> presentModes(PhysicalDevice const& physicalDevice) const;

    /**
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceCapabilitiesKHR()
     */
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const& physicalDevice) const;

    /**
     * @return allowed and available formats, in the order of @ref Config::allowedSurfaceFormats
     * @attention no format -> the surface is not usable with @ref physicalDevice
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceFormatsKHR()
     */
    [[nodiscard]] std::vector<format_t> formats(PhysicalDevice const& physicalDevice) const;

    /**
     * @return the most preferred allowed and available format
     * @throws cth::except::default_exception reason: no allowed format available
     */
    [[nodiscard]] format_t format(PhysicalDevice const& physicalDevice) const;

    /**
     * @return the most preferred allowed and available present mode
     * @throws cth::except::default_exception reason: no allowed present mode available
     */
    [[nodiscard]] present_mode_t presentMode(PhysicalDevice const& physicalDevice) const;

    static void destroy(vk_not_null<VkInstance> vkInstance, VkSurfaceKHR vkSurface);

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }
    [[nodiscard]] Config const& config() const { return _config; }

private:
    void reset();

    not_null<Instance const*> _instance;
    Config _config;

    move_ptr<VkSurfaceKHR_T> _handle = VK_NULL_HANDLE;

public:
    Surface(Surface const& other) = delete;
    Surface(Surface&& other) noexcept = default;
    Surface& operator=(Surface const& other) = delete;
    Surface& operator=(Surface&& other) noexcept = default;

    static void debug_check(Surface const& surface);
    static void debug_check_handle(vk_not_null<VkSurfaceKHR> vkSurface);
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
