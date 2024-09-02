#pragma once
#include "vulkan/base/CthInstance.hpp"



namespace cth::vk {
class PhysicalDevice;

class Instance;
class OSWindow;


class Surface {
public:
    struct State;

    /**
     * @brief base constructor
     * @param instance must be created
     * @param destruction_queue nullptr or created
     */
    Surface(not_null<Instance const*> instance, DestructionQueue* destruction_queue) :
        _instance(instance), _destructionQueue{destruction_queue} {}

    Surface(not_null<Instance const*> instance, DestructionQueue* destruction_queue, State const& state);

    ~Surface();

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief destroys and resets
     * @note if @ref DestructionQueue is set, pushes to queue
     * @note calls @ref destroy(VkInstance, VkSurfaceKHR)
     * @note @ref created() required
     */
    void destroy();

    /**
     * @brief calls @ref destroy() if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @note @ref created() required
     */
    State release();

    [[nodiscard]] bool supportsFamily(PhysicalDevice const& physical_device, uint32_t family_index) const;
    [[nodiscard]] std::vector<VkPresentModeKHR> presentModes(PhysicalDevice const& physical_device) const;
    [[nodiscard]] std::vector<VkSurfaceFormatKHR> formats(PhysicalDevice const& physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(PhysicalDevice const& physical_device) const;


    /**
     * @brief creates an invisible surface
     * @note should only be used in temp context
     */
    static Surface Temp(not_null<Instance const*> instance, DestructionQueue* destruction_queue = nullptr);


    /**
     * @brief destroys the @ref VkSurfaceKHR
     */
    static void destroy(vk::not_null<VkInstance> instance, VkSurfaceKHR surface);

private:
    void reset();

    not_null<Instance const*> _instance;
    DestructionQueue* _destructionQueue;

    move_ptr<VkSurfaceKHR_T> _handle = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR get() const { return _handle.get(); }

    Surface(Surface const& other) = default;
    Surface(Surface&& other) noexcept = delete;
    Surface& operator=(Surface const& other) = default;
    Surface& operator=(Surface&& other) noexcept = delete;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<Surface const*> surface);
    static void debug_check_handle(vk::not_null<VkSurfaceKHR> surface);
#define DEBUG_CHECK_SURFACE(surface_ptr) Surface::debug_check(surface_ptr)
#define DEBUG_CHECK_SURFACE_HANDLE(surface) Surface::debug_check_handle(surface)
#else
#define DEBUG_CHECK_SURFACE(surface_ptr) ((void)0)
#define DEBUG_CHECK_SURFACE_HANDLE(surface) ((void)0)
#endif
};
}

//State

namespace cth::vk {
struct Surface::State {
    vk::not_null<VkSurfaceKHR> vkSurface;
};

}
