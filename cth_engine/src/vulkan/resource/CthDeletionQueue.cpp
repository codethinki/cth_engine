#include "CthDeletionQueue.hpp"

#include "memory/CthBasicMemory.hpp"

#include<cth/cth_variant.hpp>

#include "buffer/CthBasicBuffer.hpp"
#include "image/CthBasicImage.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"

namespace cth {

DeletionQueue::DeletionQueue(Device* device) : _device(device) {}
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
            [this](VkDeviceMemory memory) { BasicMemory::free(_device, memory); },
            [this](VkBuffer buffer) { BasicBuffer::destroy(_device, buffer); },
            [this](VkImage image) { BasicImage::destroy(_device, image); },
            [this](VkSemaphore semaphore) { BasicSemaphore::destroy(_device, semaphore); },
        }, handle);
    }
    handles.clear();
}

#ifdef _DEBUG
void DeletionQueue::debug_check(const DeletionQueue* queue) {
    CTH_ERR(queue == nullptr, "queue must not be nullptr") throw details->exception();
}
#endif

}
