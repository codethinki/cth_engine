#include "CthImage.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/resource/memory/CthMemory.hpp"


namespace cth {
using namespace std;


Image::Image(const BasicCore* core, DeletionQueue* deletion_queue, const VkExtent2D extent, const BasicImage::Config& config,
    const VkMemoryPropertyFlags memory_properties) : BasicImage(core, extent, config), _deletionQueue(deletion_queue) {
    BasicMemory* memory = new Memory(core, _deletionQueue, memory_properties);
    DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue);

    BasicImage::create();
    BasicImage::alloc(memory);
    BasicImage::bind();
}
Image::~Image() {
    if(get() != VK_NULL_HANDLE) Image::destroy();
    if(_state.memory) destroyMemory();
}
void Image::wrap(VkImage vk_image, const State& state) {
    if(get() != VK_NULL_HANDLE) destroy();
    if(_state.memory) destroyMemory();

    BasicImage::wrap(vk_image, state);
}

void Image::create() {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicImage::create();
}



void Image::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    if(_state.memory->allocated()) _state.memory->free(deletion_queue);
    BasicImage::destroy(_deletionQueue);
}
void Image::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();
    if(_state.memory) destroyMemory();
    BasicImage::setMemory(new_memory);
}
void Image::destroyMemory(DeletionQueue* deletion_queue) {
    CTH_ERR(_state.memory == nullptr, "memory invalid") throw details->exception();
    if(_state.memory->allocated()) _state.memory->free(deletion_queue);
    delete _state.memory.get();
    _state.memory = nullptr;
}


} // namespace cth
