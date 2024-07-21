#include "CthMemory.hpp"


//Memory
namespace cth::vk {
Memory::Memory(BasicCore const* core, DeletionQueue* deletion_queue, VkMemoryPropertyFlags const properties) : BasicMemory(core, properties),
    _deletionQueue(deletion_queue) {}
Memory::Memory(BasicCore const* core, DeletionQueue* deletion_queue, VkMemoryPropertyFlags const properties, size_t const size, VkDeviceMemory memory) :
    BasicMemory(core, properties, size, memory), _deletionQueue(deletion_queue) {}

Memory::~Memory() { if(get() != VK_NULL_HANDLE) Memory::free(); }

void Memory::alloc(VkMemoryRequirements const& requirements) {
    if(get() != VK_NULL_HANDLE) Memory::free();
    BasicMemory::alloc(requirements);
}
void Memory::free(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicMemory::free(_deletionQueue);
}
void Memory::reset() { Memory::free(); }
}
