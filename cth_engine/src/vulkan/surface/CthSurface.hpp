#pragma once
#include "vulkan/base/CthInstance.hpp"



namespace cth::vk {
class PhysicalDevice;

class Instance;
class OSWindow;


class Surface {
public:
    explicit Surface(BasicInstance const* instance, VkSurfaceKHR vk_surface) : _handle(vk_surface), _instance(instance) {}
    ~Surface();

    [[nodiscard]] bool supportsFamily(PhysicalDevice const& physical_device, uint32_t family_index) const;
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(PhysicalDevice const& physical_device) const;
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(PhysicalDevice const& physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const& physical_device) const;


    static Surface Temp(BasicInstance const* instance);

private:
    move_ptr<VkSurfaceKHR_T> _handle = VK_NULL_HANDLE;

    BasicInstance const* _instance;

public:
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }

    Surface(Surface const& other) = default;
    Surface(Surface&& other) noexcept = delete;
    Surface& operator=(Surface const& other) = default;
    Surface& operator=(Surface&& other) noexcept = delete;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(Surface const* surface);
#define DEBUG_CHECK_SURFACE(surface_ptr) Surface::debug_check(surface_ptr)
    #else
#define DEBUG_CHECK_SURFACE(surface_ptr) ((void)0)
#endif
};
}
