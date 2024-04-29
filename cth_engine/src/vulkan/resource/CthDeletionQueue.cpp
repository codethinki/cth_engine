#include "CthDeletionQueue.hpp"

#include "CthMemory.hpp"

#include<cth/cth_variant.hpp>

#include "buffer/CthBasicBuffer.hpp"
#include "image/CthBasicImage.hpp"

namespace cth {

DeletionQueue::DeletionQueue(Device* device) : device(device) {}
DeletionQueue::~DeletionQueue() {
    for(uint32_t i = 0; i < QUEUES; ++i) 
        clear((frame + i) % QUEUES);
}

void DeletionQueue::push(const deletable_handle_t handle) {
    CTH_WARN(std::visit(var::visitor{ [](auto handle){ return handle == VK_NULL_HANDLE; } }, handle), "handle invalid");
    queue_[frame].push_back(handle);
}
void DeletionQueue::clear(const uint32_t current_frame) {

    auto& handles = queue_[current_frame];
    for(auto handle : handles) {
        std::visit(cth::var::overload{
            [this](VkDeviceMemory memory) { BasicMemory::free(device, memory); },
            [this](VkBuffer buffer) { BasicBuffer::destroy(device, buffer); },
            [this](VkImage image) { BasicImage::destroy(device, image); },
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
