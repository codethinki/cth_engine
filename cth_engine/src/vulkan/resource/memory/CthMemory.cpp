#include "CthMemory.hpp"


//Memory
namespace cth::vk {
Memory::Memory(BasicCore const* core, DestructionQueue* destruction_queue, VkMemoryPropertyFlags const properties) : BasicMemory(core, properties),
    _destructionQueue(destruction_queue) {}
Memory::Memory(BasicCore const* core, DestructionQueue* destruction_queue, VkMemoryPropertyFlags const properties, size_t const size, VkDeviceMemory memory) :
    BasicMemory(core, properties, size, memory), _destructionQueue(destruction_queue) {}

Memory::~Memory() { if(get() != VK_NULL_HANDLE) Memory::free(); }

void Memory::alloc(VkMemoryRequirements const& requirements) {
    if(get() != VK_NULL_HANDLE) Memory::free();
    BasicMemory::alloc(requirements);
}
void Memory::free(DestructionQueue* destruction_queue) {
    if(destruction_queue) _destructionQueue = destruction_queue;
    BasicMemory::free(_destructionQueue);
}
void Memory::reset() { Memory::free(); }
}
