#include "CthDeletionQueue.hpp"

#include "buffer/CthBasicBuffer.hpp"
#include "image/CthBasicImage.hpp"
#include "memory/CthBasicMemory.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"


#ifdef CONSTANT_DEBUG_MODE
#include "vulkan/debug/CthBasicDebugMessenger.hpp"
#endif

#include<cth/cth_variant.hpp>

#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/surface/CthBasicSwapchain.hpp"



namespace cth {

DeletionQueue::DeletionQueue(BasicCore* core) : _core(core) {}
DeletionQueue::~DeletionQueue() {
    for(uint32_t i = 0; i < QUEUES; ++i)
        clear((_frame + i) % QUEUES);
}
void DeletionQueue::push(deletable_handle_t handle) {
    CTH_WARN(std::visit(var::visitor{[](auto handle) { return handle == VK_NULL_HANDLE; }}, handle),
        "handle should not be VK_NULL_HANDLE or nullptr");

    _queue[_frame].emplace_back(handle);
}
void DeletionQueue::push(dependent_handle_t handle, deletable_handle_t dependency) {
    const bool validHandle = std::visit(var::visitor{[](auto temp_handle) { return temp_handle != VK_NULL_HANDLE; }}, handle);
    const bool validDependency = std::visit(var::visitor{[](auto temp_dependency) { return temp_dependency == VK_NULL_HANDLE; }}, dependency);
    CTH_WARN(!validHandle, "handle should not be VK_NULL_HANDLE or nullptr");
    CTH_WARN(!validDependency, "dependency should not be VK_NULL_HANDLE or nullptr");

    CTH_ERR(validHandle ^ validDependency, "handle and dependency must both be valid or invalid") throw details->exception();

    _queue[_frame].emplace_back(handle, dependency);
}



void DeletionQueue::clear(const uint32_t current_frame) {

    auto& deletables = _queue[current_frame];
    for(auto [deletable, dependency] : deletables) {
        std::visit(cth::var::overload{
            [this](deletable_handle_t handle) {
                std::visit(cth::var::overload{
                    [this](VkDeviceMemory vk_memory) { BasicMemory::free(_core->vkDevice(), vk_memory); },
                    [this](VkBuffer vk_buffer) { BasicBuffer::destroy(_core->vkDevice(), vk_buffer); },
                    [this](VkImage vk_image) { BasicImage::destroy(_core->vkDevice(), vk_image); },
                    [this](VkSemaphore vk_semaphore) { BasicSemaphore::destroy(_core->vkDevice(), vk_semaphore); },
                    [this](VkFence vk_fence) { BasicFence::destroy(_core->vkDevice(), vk_fence); },
                    [this](VkCommandPool vk_cmd_pool) { CmdPool::destroy(_core->vkDevice(), vk_cmd_pool); },
                    [this](VkSwapchainKHR vk_swapchain) { BasicSwapchain::destroy(_core->vkDevice(), vk_swapchain); },
                }, handle);
            },
            //device deletable

            [this, dependency](dependent_handle_t dependent) {
                std::visit(cth::var::overload{
                    [this, dependency](VkCommandBuffer vk_cmd_buffer) {
                        CmdBuffer::free(_core->vkDevice(), std::get<VkCommandPool>(dependency), vk_cmd_buffer);
                    },
                }, dependent);
            },
        }, deletable);
    }
    deletables.clear();
}

#ifdef CONSTANT_DEBUG_MODE
void DeletionQueue::debug_check(const DeletionQueue* queue) {
    CTH_ERR(queue == nullptr, "queue must not be nullptr") throw details->exception();
}
void DeletionQueue::debug_check_null_allowed(const DeletionQueue* queue) { if(queue) debug_check(queue); }
#endif

}
