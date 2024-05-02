#include "CthMemory.hpp"


//Memory
namespace cth {
Memory::Memory(Device* device, DeletionQueue* deletion_queue, const VkMemoryPropertyFlags properties) : BasicMemory(device, properties),
    _deletionQueue(deletion_queue) {}
Memory::Memory(Device* device, DeletionQueue* deletion_queue, const VkMemoryPropertyFlags properties, const size_t size, VkDeviceMemory memory) :
    BasicMemory(device, properties, size, memory), _deletionQueue(deletion_queue) {}

Memory::~Memory() { if(get() != VK_NULL_HANDLE) Memory::free(); }

void Memory::alloc(const VkMemoryRequirements& requirements) {
    if(get() != VK_NULL_HANDLE) Memory::free();
    BasicMemory::alloc(requirements);
}
void Memory::free(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicMemory::free(_deletionQueue);
}
void Memory::reset() { Memory::free(); }
}
