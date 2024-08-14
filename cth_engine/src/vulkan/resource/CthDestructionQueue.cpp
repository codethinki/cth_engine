#include "CthDestructionQueue.hpp"

#include "buffer/CthBasicBuffer.hpp"
#include "image/CthBasicImage.hpp"
#include "memory/CthBasicMemory.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/surface/swapchain/CthBasicSwapchain.hpp"


#ifdef CONSTANT_DEBUG_MODE
#include "vulkan/debug/CthBasicDebugMessenger.hpp"
#endif



#include "interface/render/CthRenderer.hpp"

#include "vulkan/surface/CthOSWindow.hpp"
#include "vulkan/surface/CthSurface.hpp"

#include<cth/cth_variant.hpp>


namespace cth::vk {

DestructionQueue::DestructionQueue(Device* device, PhysicalDevice* physical_device, BasicInstance* instance) : _device{device}, _physicalDevice{physical_device},
    _instance{instance} {}
DestructionQueue::~DestructionQueue() {
    clear();
}
void DestructionQueue::push(destructible_handle_t handle) {
    CTH_WARN(std::visit(var::visitor{[](auto handle) { return handle == VK_NULL_HANDLE; }}, handle),
        "handle should not be VK_NULL_HANDLE or nullptr") {}

    _queue[_cycleSubIndex].emplace_back(handle);
}
void DestructionQueue::push(dependent_handle_t handle, destructible_handle_t dependency) {
    bool const validHandle = std::visit(var::visitor{[](auto temp_handle) { return temp_handle != VK_NULL_HANDLE; }}, handle);
    bool const validDependency = std::visit(var::visitor{[](auto temp_dependency) { return temp_dependency == VK_NULL_HANDLE; }}, dependency);
    CTH_WARN(!validHandle, "handle should not be VK_NULL_HANDLE or nullptr") {}
    CTH_WARN(!validDependency, "dependency should not be VK_NULL_HANDLE or nullptr") {}

    CTH_ERR(validHandle ^ validDependency, "handle and dependency must both be valid or invalid") throw details->exception();

    _queue[_cycleSubIndex].emplace_back(handle, dependency);
}



void DestructionQueue::clear(size_t const cycle_sub_index) {

    auto& deletables = _queue[cycle_sub_index];
    for(auto [deletable, dependency] : deletables) {
        std::visit(cth::var::overload{
            [this](destructible_handle_t handle) {
                std::visit(cth::var::overload{
                    //device destructible
                    [this](VkDeviceMemory vk_memory) { BasicMemory::free(_device->get(), vk_memory); },
                    [this](VkBuffer vk_buffer) { BasicBuffer::destroy(_device->get(), vk_buffer); },
                    [this](VkImage vk_image) { BasicImage::destroy(_device->get(), vk_image); },
                    [this](VkSemaphore vk_semaphore) { BasicSemaphore::destroy(_device->get(), vk_semaphore); },
                    [this](VkFence vk_fence) { BasicFence::destroy(_device->get(), vk_fence); },
                    [this](VkCommandPool vk_cmd_pool) { CmdPool::destroy(_device->get(), vk_cmd_pool); },
                    [this](VkSwapchainKHR vk_swapchain) { BasicSwapchain::destroy(_device->get(), vk_swapchain); },

                    //instance destructible
                    [this](VkSurfaceKHR vk_surface) { Surface::destroy(vk_surface, _instance->get()); },
                    [](GLFWwindow* glfw_window) { OSWindow::destroy(glfw_window); },
                }, handle);
            },

            [this, dependency](dependent_handle_t dependent) {
                std::visit(cth::var::overload{
                    [this, dependency](VkCommandBuffer vk_cmd_buffer) {
                        CmdBuffer::free(_device->get(), std::get<VkCommandPool>(dependency), vk_cmd_buffer);
                    },
                }, dependent);
            },
        }, deletable);
    }
    deletables.clear();
}
void DestructionQueue::clear() {
    for(uint32_t i = 0; i < QUEUES; ++i)
        clear((_cycleSubIndex + i) % QUEUES);
}

#ifdef CONSTANT_DEBUG_MODE
void DestructionQueue::debug_check(DestructionQueue const* queue) {
    CTH_ERR(queue == nullptr, "queue must not be nullptr") throw details->exception();
}
void DestructionQueue::debug_check_null_allowed(DestructionQueue const* queue) { if(queue) debug_check(queue); }
#endif

}
