#include "CthDebugMessenger.hpp"


namespace cth {

DebugMessenger::~DebugMessenger() {
    if(get() != VK_NULL_HANDLE) DebugMessenger::destroy();
}
void DebugMessenger::create(BasicInstance* instance) {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicDebugMessenger::create(instance);
}
void DebugMessenger::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicDebugMessenger::destroy(_deletionQueue);
}
}
