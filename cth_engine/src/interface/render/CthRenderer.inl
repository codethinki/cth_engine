#pragma once
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

namespace cth {
template<Renderer::Phase P>
PrimaryCmdBuffer* Renderer::begin() {
    DEBUG_CHECK_RENDERER_PHASE(this, P);
    auto buffer = cmdBuffer<P>();
    const VkResult beginResult = buffer->begin();
    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
        throw cth::except::vk_result_exception{beginResult, details->exception()};

    return buffer;
}
template<Renderer::Phase P>
void Renderer::end() {
    DEBUG_CHECK_RENDERER_PHASE(this, P);
    //CTH_ERR(!_transferStarted, "no frame active") throw details->exception();

    const PrimaryCmdBuffer* buffer = cmdBuffer<P>();

    const VkResult recordResult = buffer->end();
    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
        throw cth::except::vk_result_exception{recordResult, details->exception()};


    submit<P>();
    //const VkResult submitResult = _swapchain->submitCommandBuffer(_deletionQueue, cmdBuffer, _currentImageIndex);

}
template<Renderer::Phase P> void Renderer::skip() {
    DEBUG_CHECK_RENDERER_PHASE(this, P);
    nextState<P>();
}

template<Renderer::Phase P>
void Renderer::submit() {
    DEBUG_CHECK_RENDERER_PHASE(this, P);

    queue<P>()->submit();
}



template<Renderer::Phase P>
const Queue* Renderer::queue() const {
    return _queues[P];
}
template<Renderer::Phase P>
const PrimaryCmdBuffer* Renderer::cmdBuffer() const {
    static_assert(P == PHASE_MAX_ENUM, "Phase P must not be PHASE_MAX_ENUM");
    return _cmdBuffers[_currentFrameIndex * PHASE_MAX_ENUM + P];
}

template<Renderer::Phase P> void Renderer::nextState() {
    DEBUG_CHECK_RENDERER_PHASE(this, P);

    if constexpr (P == PHASE_LAST) _state = PHASE_FIRST; 
    else _state = static_cast<Phase>(static_cast<size_t>(_state) + 1);
    
}



#ifdef CONSTANT_DEBUG_MODE
template<Renderer::Phase P>
void Renderer::debug_check_phase(const Renderer* renderer) {
    static_assert(P == PHASE_MAX_ENUM, "Phase P must not be PHASE_MAX_ENUM");
    CTH_ERR(static_cast<size_t>(P) != static_cast<size_t>(renderer->_state), "phase error, explicitly skip phases") throw details->exception();

}
#endif

}
