#pragma once
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <array>
#include <variant>
#include <vector>


namespace cth::vk {
struct Cycle;
}

namespace cth::vk {
class BasicInstance;
class Device;
class PhysicalDevice;

class DestructionQueue {
public:
    using destructible_handle_t = std::variant<
        VkDeviceMemory, VkBuffer, VkImage,
        VkImageView,
        VkSemaphore, VkFence,
        VkCommandPool,
        VkRenderPass,
        VkFramebuffer,
        VkSwapchainKHR,
        VkSurfaceKHR,
        GLFWwindow*>;

    using dependent_handle_t = std::variant<
        VkCommandBuffer
    >;

    explicit DestructionQueue(Device* device, PhysicalDevice* physical_device, BasicInstance* instance);
    ~DestructionQueue();

    void push(destructible_handle_t handle);
    void push(dependent_handle_t handle, destructible_handle_t dependency);


    void clear();
    void clear(size_t cycle_sub_index);

private:
    using handle_t = std::variant<
        destructible_handle_t,
        dependent_handle_t
    >;
    struct destructible {
        handle_t handle;
        destructible_handle_t dependency;
    };

    static constexpr size_t QUEUES = constants::FRAMES_IN_FLIGHT;

    size_t _cycleSubIndex = 0;

    Device* _device;
    PhysicalDevice* _physicalDevice;
    BasicInstance* _instance;

    std::array<std::vector<destructible>, QUEUES> _queue;

public:
    DestructionQueue(DestructionQueue const& other) = delete;
    DestructionQueue(DestructionQueue&& other) noexcept = default;
    DestructionQueue& operator=(DestructionQueue const& other) = delete;
    DestructionQueue& operator=(DestructionQueue&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(DestructionQueue const* queue);
    static void debug_check_null_allowed(DestructionQueue const* queue);
#define DEBUG_CHECK_DESTRUCTION_QUEUE(destruction_queue_ptr) DestructionQueue::debug_check(destruction_queue_ptr)
#define DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue_ptr) 
#else
#define DEBUG_CHECK_DESTRUCTION_QUEUE(destruction_queue_ptr) ((void)0)
#define DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue_ptr) ((void)0) 
#endif
};

}
