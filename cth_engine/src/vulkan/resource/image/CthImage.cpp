#include "CthImage.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/resource/memory/CthMemory.hpp"


namespace cth::vk {
using namespace std;


Image::Image(BasicCore const* core, DestructionQueue* destruction_queue, VkExtent2D const extent, BasicImage::Config const& config,
    VkMemoryPropertyFlags const memory_properties) : BasicImage(core, extent, config), _destructionQueue(destruction_queue) {
    BasicMemory* memory = new Memory(core, _destructionQueue, memory_properties);
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue);

    BasicImage::create();
    BasicImage::alloc(memory);
    BasicImage::bind();
}
Image::~Image() {
    if(get() != VK_NULL_HANDLE) Image::destroy();
    if(_state.memory) destroyMemory();
}
void Image::wrap(VkImage vk_image, State const& state) {
    if(get() != VK_NULL_HANDLE) destroy();
    if(_state.memory) destroyMemory();

    BasicImage::wrap(vk_image, state);
}

void Image::create() {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicImage::create();
}



void Image::destroy(DestructionQueue* destruction_queue) {
    if(destruction_queue) _destructionQueue = destruction_queue;
    if(_state.memory->allocated()) _state.memory->free(destruction_queue);
    BasicImage::destroy(_destructionQueue);
}
void Image::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();
    if(_state.memory) destroyMemory();
    BasicImage::setMemory(new_memory);
}
void Image::destroyMemory(DestructionQueue* destruction_queue) {
    CTH_ERR(_state.memory == nullptr, "memory invalid") throw details->exception();
    if(_state.memory->allocated()) _state.memory->free(destruction_queue);
    delete _state.memory.get();
    _state.memory = nullptr;
}


} // namespace cth
