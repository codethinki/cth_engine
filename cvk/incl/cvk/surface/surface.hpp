#pragma once
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/handle/instance_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <functional>
#include <vector>


namespace cvk {
class Instance;
class PhysicalDevice;

/**
 * `VkSurfaceKHR` wrapper
 * @details the window system creates the surface, `wrap()` takes it
 */
class Surface {
public:
    struct State;

    /**
     * gives the extent of the window of the surface in framebuffer pixels
     */
    using extent_callback_t = std::function<VkExtent2D()>;

    /**
     * base constructor
     */
    explicit Surface(Instance const&);

    /**
     * constructs and wraps
     * @details calls @ref Surface(Instance const&), `wrap()`
     */
    Surface(Instance const&, State const&);

    /**
     * @note calls `optDestroy()`
     */
    ~Surface() { optDestroy(); }

    /**
     * wraps the state
     * @pre `Instance::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * destroys and forgets
     * @pre `created()`
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    /**
     * @return queue family `familyIndex` can present to this surface
     * @throws cvk::vk_result_exception result of `vkGetPhysicalDeviceSurfaceSupportKHR()`
     */
    [[nodiscard]] bool supportsFamily(PhysicalDevice const&, uint32_t familyIndex) const;

    /**
     * @return present modes the physical device offers for the surface
     * @throws cvk::vk_result_exception result of `vkGetPhysicalDeviceSurfacePresentModesKHR()`
     */
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(PhysicalDevice const&) const;

    /**
     * @throws cvk::vk_result_exception result of `vkGetPhysicalDeviceSurfaceCapabilitiesKHR()`
     */
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const&) const;

    /**
     * @return formats the physical device offers for the surface
     * @attention no format -> the surface is not usable with the physical device
     * @throws cvk::vk_result_exception result of `vkGetPhysicalDeviceSurfaceFormatsKHR()`
     */
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(PhysicalDevice const&) const;

    /**
     * @return extent of the swapchain images for the surface
     * @details
     * - `currentExtent` of the capabilities
     * - an undefined `currentExtent` (Wayland, headless): `State::extentCallback`, clamped to `minImageExtent` and
     *   `maxImageExtent`
     * - a width or height `0` of the callback stays `0`: no image
     * @pre `created()`
     * @pre an undefined `currentExtent` -> the state has an extent callback
     * @throws cvk::vk_result_exception result of `vkGetPhysicalDeviceSurfaceCapabilitiesKHR()`
     * @see `Swapchain::evalExtent()`
     */
    [[nodiscard]] VkExtent2D extent(PhysicalDevice const&) const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkSurfaceKHR get() const { return _handle.get(); }

private:
    void forget();

    cth::ref_wrapper<Instance const> _instance;
    instance_handle<VkSurfaceKHR> _handle{nullptr};
    extent_callback_t _extentCallback{};

public:
    Surface(Surface const& other) = delete;
    Surface(Surface&& other) noexcept = default;
    Surface& operator=(Surface const& other) = delete;
    Surface& operator=(Surface&& other) noexcept = default;

    static constexpr void debug_check(Surface const&);
    static constexpr void debug_check_handle(vk_not_null<VkSurfaceKHR>);
};
}

namespace cvk {
struct Surface::State {
    vk_not_null<VkSurfaceKHR> vkSurface;

    extent_callback_t extentCallback{}; ///< for an undefined `currentExtent` (Wayland, headless)
};
}

namespace cvk {
constexpr void Surface::debug_check(Surface const& surface) {
    CTH_CRITICAL(!surface.created(), "surface must be created") {}
    debug_check_handle(surface.get());
}
constexpr void Surface::debug_check_handle([[maybe_unused]] vk_not_null<VkSurfaceKHR> vkSurface) {}
}
