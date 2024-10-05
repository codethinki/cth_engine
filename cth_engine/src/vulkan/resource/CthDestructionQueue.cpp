#include "CthDestructionQueue.hpp"

#include "buffer/CthBaseBuffer.hpp"
#include "image/CthImage.hpp"
#include "memory/CthMemory.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/render/cmd/CthCmdPool.hpp"
#include "vulkan/render/control/CthFence.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/image/Framebuffer.hpp"
#include "vulkan/surface/swapchain/CthBasicSwapchain.hpp"


#ifdef CONSTANT_DEBUG_MODE
#include "vulkan/debug/CthDebugMessenger.hpp"
#endif



#include "image/CthImageView.hpp"
#include "image/texture/CthSampler.hpp"

#include "interface/render/CthRenderer.hpp"

#include "vulkan/render/pass/CthRenderPass.hpp"
#include "vulkan/surface/CthOSWindow.hpp"
#include "vulkan/surface/CthSurface.hpp"

#include <cth/variant.hpp>


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

#ifdef CONSTANT_DEBUG_MODE
void DestructionQueue::debug_check(DestructionQueue const* queue) {
    CTH_ERR(queue == nullptr, "queue must not be nullptr") throw details->exception();
}
void DestructionQueue::debug_check_null_allowed(DestructionQueue const* queue) { if(queue) debug_check(queue); }
#endif

}
