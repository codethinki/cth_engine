#include "CthDeletionQueue.hpp"

#include "buffer/CthBasicBuffer.hpp"
#include "image/CthBasicImage.hpp"
#include "memory/CthBasicMemory.hpp"
#include "vulkan/base/CthContext.hpp"
#include "vulkan/base/CthInstance.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"

#ifdef CONSTANT_DEBUG_MODE
#include "vulkan/debug/CthBasicDebugMessenger.hpp"
#endif

#include<cth/cth_variant.hpp>



namespace cth {

DeletionQueue::DeletionQueue(Context* context) : _context(context) {}
DeletionQueue::~DeletionQueue() {
    for(uint32_t i = 0; i < QUEUES; ++i)
        clear((_frame + i) % QUEUES);
}

void DeletionQueue::push(const deletable_handle_t handle) {
    CTH_WARN(std::visit(var::visitor{ [](auto handle){ return handle == VK_NULL_HANDLE; } }, handle), "handle invalid");
    _queue[_frame].push_back(handle);
}
void DeletionQueue::clear(const uint32_t current_frame) {

    auto& handles = _queue[current_frame];
    for(auto handle : handles) {
        std::visit(cth::var::overload{
            //resource
            [this](VkDeviceMemory vk_memory) { BasicMemory::free(_context->device(), vk_memory); },
            [this](VkBuffer vk_buffer) { BasicBuffer::destroy(_context->device(), vk_buffer); },
            [this](VkImage vk_image) { BasicImage::destroy(_context->device(), vk_image); },
            //control
            [this](VkSemaphore vk_semaphore) { BasicSemaphore::destroy(_context->device(), vk_semaphore); },
            //base
            [](VkInstance vk_instance) { BasicInstance::destroy(vk_instance); },
            //debug
            [this](VkDebugUtilsMessengerEXT vk_messenger) { BasicDebugMessenger::destroy(_context->instance(), vk_messenger); }


        }, handle);
    }
    handles.clear();
}

#ifdef CONSTANT_DEBUG_MODE
void DeletionQueue::debug_check(const DeletionQueue* queue) {
    CTH_ERR(queue == nullptr, "queue must not be nullptr") throw details->exception();
}
#endif

}
