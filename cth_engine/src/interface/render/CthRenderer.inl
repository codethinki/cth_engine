#pragma once

#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include <ranges>

namespace cth {
template<Renderer::Phase P>
PrimaryCmdBuffer* Renderer::begin() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);
    auto buffer = cmdBuffer<P>();
    const VkResult beginResult = buffer->begin();
    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
        throw except::vk_result_exception{beginResult, details->exception()};

    return buffer;
}
template<Renderer::Phase P>
void Renderer::end() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);
    //CTH_ERR(!_transferStarted, "no frame active") throw details->exception();

    const PrimaryCmdBuffer* buffer = cmdBuffer<P>();

    const VkResult recordResult = buffer->end();
    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
        throw except::vk_result_exception{recordResult, details->exception()};


    submit<P>();
    //const VkResult submitResult = _swapchain->submitCommandBuffer(_deletionQueue, cmdBuffer, _currentImageIndex);

}
template<Renderer::Phase P> void Renderer::skip() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);
    nextState<P>();
}

template<Renderer::Phase P>
void Renderer::submit() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    queue<P>()->submit();
    nextState<P>();
}



template<Renderer::Phase P>
const Queue* Renderer::queue() const { return _queues[P]; }
template<Renderer::Phase P>
const PrimaryCmdBuffer* Renderer::cmdBuffer() const {
    static_assert(P == PHASE_MAX_ENUM, "Phase P must not be PHASE_MAX_ENUM");
    return _cmdBuffers[_currentFrameIndex * PHASE_MAX_ENUM + P].get();
}

template<Renderer::Phase P> void Renderer::nextState() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    if constexpr(P == PHASE_LAST) _state = PHASE_FIRST;
    else _state = static_cast<Phase>(static_cast<size_t>(_state) + 1);

}



#ifdef CONSTANT_DEBUG_MODE
template<Renderer::Phase P>
void Renderer::debug_check_phase() {
    static_assert(P != PHASE_MAX_ENUM, "Phase P must not be PHASE_MAX_ENUM");
    static_assert(PHASE_FIRST == PHASE_TRANSFER && PHASE_LAST == PHASE_RENDER && PHASE_TRANSFER == 0 && PHASE_RENDER == 1,
        "enum values changed please adjust checks");
}


template<Renderer::Phase P>
void Renderer::debug_check_phase_change(const Renderer* renderer) {
    DEBUG_CHECK_RENDERER_PHASE(P);
    CTH_ERR(static_cast<size_t>(P) != static_cast<size_t>(renderer->_state), "phase error, explicitly skip phases") throw details->exception();

}
#endif

}

//Builder

namespace cth {
template<Renderer::Phase P>
auto Renderer::Config::addSignalSet(const std::span<BasicSemaphore* const, SET_SIZE> signal_semaphore_set) -> Config& {
    add<P>(signal_semaphore_set, _signalSets);

    return *this;
}
template<Renderer::Phase P>
auto Renderer::Config::addWaitSet(const std::span<const PipelineWaitStage, SET_SIZE> wait_stage_set) -> Config& {
    add<P>(wait_stage_set, _waitSets);


    return *this;

}

template<Renderer::Phase P>
auto Renderer::Config::removeSignalSet(const std::span<BasicSemaphore* const, SET_SIZE> signal_semaphores) -> Config& {

    remove<P>(signal_semaphores, _signalSets);
    return *this;
}
template<Renderer::Phase P>
Renderer::Config& Renderer::Config::removeWaitSet(std::span<const VkPipelineStageFlags> wait_stages) {
    remove<P>(wait_stages, _waitSets);
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addQueue(const Queue* queue) -> Config& {
    DEBUG_CHECK_QUEUE(queue);
    DEBUG_CHECK_RENDERER_PHASE(P)

    _queues[P] = queue;
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::removeQueue(const Queue* queue) -> Config& {
    DEBUG_CHECK_QUEUE(queue);
    DEBUG_CHECK_RENDERER_PHASE(P)

    _queues[P] = nullptr;
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addPhase(const Queue* queue, std::optional<std::span<BasicSemaphore* const>> signal_semaphore_sets,
    std::optional<std::span<const PipelineWaitStage>> wait_stage_set) -> Config& {
    addQueue<P>(queue);

    if(signal_semaphore_sets.has_value()) addSignalSet<P>(*signal_semaphore_sets);
    if(wait_stage_set.has_value()) addWaitSet<P>(*wait_stage_set);
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::createPhaseSubmitInfo(std::span<const PrimaryCmdBuffer* const, SET_SIZE> cmd_buffers) -> std::array<Queue::SubmitInfo, SET_SIZE> {
    //TEMP complete this
    //TEMP init the SubmitInfo and then put the phases together in the createSubmitInfo function

    DEBUG_CHECK_RENDERER_PHASE(P);
    std::array<Queue::SubmitInfo, SET_SIZE> submitInfos{};

    for(size_t i = 0; i < SET_SIZE; i++) {
        
    }
}



template<Renderer::Phase P, class T>
auto Renderer::Config::add(std::span<T const, SET_SIZE> signal_semaphores, collection_t<T>& to) -> void {
    DEBUG_CHECK_RENDERER_PHASE(P)

        auto& set = to[P];

    set.emplace_back();
    std::ranges::copy(signal_semaphores, set.back().begin());
}

template<Renderer::Phase P, class T>
auto Renderer::Config::remove(std::span<T const, SET_SIZE> signal_semaphores, collection_t<T>& to) -> void {
    DEBUG_CHECK_RENDERER_PHASE(P)

        auto& set = to[P];

    const auto it = std::ranges::find_first_of(set, [signal_semaphores](std::span<T const, SET_SIZE> semaphores) {
        bool eq = true;
        for(auto [a, b] : std::views::zip(semaphores, signal_semaphores)) eq = eq && a == b;
        return eq;
        });

    set.erase(it);
}

}
