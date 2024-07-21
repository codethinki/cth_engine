#include "CthDebugMessenger.hpp"


namespace cth::vk {

DebugMessenger::~DebugMessenger() {
    if(get() != VK_NULL_HANDLE) DebugMessenger::destroy();
}
void DebugMessenger::create(BasicInstance const* instance) {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicDebugMessenger::create(instance);
}

}
