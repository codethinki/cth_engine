#pragma once
#include "vulkan/base/CthInstance.hpp"



namespace cth {
class PhysicalDevice;

class Instance;
class OSWindow;


class Surface {
public:
    explicit Surface(const BasicInstance* instance, VkSurfaceKHR vk_surface) : _handle(vk_surface), _instance(instance) {}
    ~Surface();

    [[nodiscard]] bool supportsFamily(const PhysicalDevice& physical_device, uint32_t family_index) const;
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(const PhysicalDevice& physical_device) const;
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(const PhysicalDevice& physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(const PhysicalDevice& physical_device) const;


    static Surface Temp(const BasicInstance* instance);

private:
    move_ptr<VkSurfaceKHR_T> _handle = VK_NULL_HANDLE;

    const BasicInstance* _instance;

public:
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }

    Surface(const Surface& other) = default;
    Surface(Surface&& other) noexcept = delete;
    Surface& operator=(const Surface& other) = default;
    Surface& operator=(Surface&& other) noexcept = delete;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(const Surface* surface);
#define DEBUG_CHECK_SURFACE(surface_ptr) Surface::debug_check(surface_ptr)
    #else
#define DEBUG_CHECK_SURFACE(surface_ptr) ((void)0)
#endif
};
}
