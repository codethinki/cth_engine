#pragma once
#include "vulkan/base/CthInstance.hpp"



namespace cth::vk {
class PhysicalDevice;

class Instance;
class OSWindow;


class Surface {
public:
    explicit Surface(BasicInstance const* instance, DestructionQueue* destruction_queue, VkSurfaceKHR vk_surface) : _instance(instance),
        _destructionQueue{destruction_queue}, _handle(vk_surface) {}
    ~Surface();

    void destroy(DestructionQueue* destruction_queue = nullptr);

    [[nodiscard]] bool supportsFamily(PhysicalDevice const& physical_device, uint32_t family_index) const;
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(PhysicalDevice const& physical_device) const;
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(PhysicalDevice const& physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const& physical_device) const;


    static Surface Temp(BasicInstance const* instance, DestructionQueue* destruction_queue = nullptr);
    static void destroy(VkSurfaceKHR surface, VkInstance instance);

private:
    BasicInstance const* _instance;
    DestructionQueue* _destructionQueue;

    move_ptr<VkSurfaceKHR_T> _handle = VK_NULL_HANDLE;


public:
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }

    Surface(Surface const& other) = default;
    Surface(Surface&& other) noexcept = delete;
    Surface& operator=(Surface const& other) = default;
    Surface& operator=(Surface&& other) noexcept = delete;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<Surface const*> surface);
    static void debug_check_handle(VkSurfaceKHR surface);
#define DEBUG_CHECK_SURFACE(surface_ptr) Surface::debug_check(surface_ptr)
#define DEBUG_CHECK_SURFACE_HANDLE(surface) Surface::debug_check_handle(surface)
#else
#define DEBUG_CHECK_SURFACE(surface_ptr) ((void)0)
#define DEBUG_CHECK_SURFACE_HANDLE(surface) ((void)0)
#endif
};
}
