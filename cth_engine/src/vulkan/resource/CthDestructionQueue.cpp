#include "CthDestructionQueue.hpp"

#include "buffer/CthBaseBuffer.hpp"
#include "vulkan/resource/image/Framebuffer.hpp"
#include "vulkan/surface/swapchain/CthBasicSwapchain.hpp"


#include "vulkan/debug/CthDebugMessenger.hpp"

#include "interface/render/CthRenderer.hpp"

#include "vulkan/surface/CthOSWindow.hpp"


namespace cth::vk {


DestructionQueue::~DestructionQueue() { clear(); }

void DestructionQueue::push(function_t const& function) {
    _queue[_cycleSubIndex].emplace_back(function);
}
void DestructionQueue::push(std::span<function_t const> functions) {
    for(auto& function : functions) push(function);
}



void DestructionQueue::clear(size_t  cycle_sub_index) {
    auto& deletables = _queue[cycle_sub_index];
    for(auto& deletable : deletables) deletable();
    deletables.clear();
}
void DestructionQueue::clear() {
    for(uint32_t i = 0; i < QUEUES; ++i)
        clear((_cycleSubIndex + i) % QUEUES);
}

}
