#pragma once
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/handle/instance_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <vector>


namespace cvk {
class Instance;
class PhysicalDevice;

/**
 * VkSurfaceKHR wrapper
 * @details the window system creates the surface, @ref wrap() takes it
 */
class Surface {
public:
    struct State;

    /**
     * base constructor
     * @pre @ref Instance::created()
     */
    explicit Surface(Instance const&);

    /**
     * constructs and wraps
     * @details calls:
     * - @ref Surface(Instance const&)
     * - @ref wrap()
     */
    Surface(Instance const&, State const&);

    /**
     * @note calls @ref optDestroy()
     */
    ~Surface() { optDestroy(); }

    /**
     * wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @return queue family @ref familyIndex can present to this surface
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceSupportKHR()
     */
    [[nodiscard]] bool supportsFamily(PhysicalDevice const&, uint32_t familyIndex) const;

    /**
     * @return present modes the physical device offers for the surface
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfacePresentModesKHR()
     */
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(PhysicalDevice const&) const;

    /**
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceCapabilitiesKHR()
     */
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const&) const;

    /**
     * @return formats the physical device offers for the surface
     * @attention no format -> the surface is not usable with the physical device
     * @throws cvk::vk_result_exception result of vkGetPhysicalDeviceSurfaceFormatsKHR()
     */
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(PhysicalDevice const&) const;

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkSurfaceKHR get() const { return _handle.get(); }

private:
    void forget();

    instance_handle<VkSurfaceKHR> _handle;

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
};
}

namespace cvk {
constexpr void Surface::debug_check(Surface const& surface) {
    CTH_CRITICAL(!surface.created(), "surface must be created") {}
    debug_check_handle(surface.get());
}
constexpr void Surface::debug_check_handle([[maybe_unused]] vk_not_null<VkSurfaceKHR> vkSurface) {}
}
