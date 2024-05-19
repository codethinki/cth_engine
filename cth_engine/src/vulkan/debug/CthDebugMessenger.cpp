#include "CthDebugMessenger.hpp"


namespace cth {

DebugMessenger::~DebugMessenger() {
    if(get() != VK_NULL_HANDLE) DebugMessenger::destroy();
}
void DebugMessenger::create(const BasicInstance* instance) {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicDebugMessenger::create(instance);
}

}
