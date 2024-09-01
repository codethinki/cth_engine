#pragma once

#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"

#include <cth/io/cth_log.hpp>

#include <ranges>
#include <vector>


namespace cth::vk {
template<Renderer::Phase P>
PrimaryCmdBuffer* Renderer::begin() const {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    PrimaryCmdBuffer* buffer = cmdBuffer<P>();
    buffer->begin();

    return buffer;
}
template<Renderer::Phase P>
void Renderer::end() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    PrimaryCmdBuffer const* buffer = cmdBuffer<P>();

    buffer->end();

    submit<P>();
}
template<Renderer::Phase P> void Renderer::skip() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    queue<P>()->skip(submitInfo<P>());

    nextState<P>();
}

template<Renderer::Phase P>
void Renderer::submit() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    queue<P>()->submit(submitInfo<P>());

    nextState<P>();
}



template<Renderer::Phase P>
Queue const* Renderer::queue() const { return _queues[P]; }
template<Renderer::Phase P>
PrimaryCmdBuffer* Renderer::cmdBuffer() const {
    DEBUG_CHECK_RENDERER_CURRENT_PHASE(this, P);
    return _cmdBuffers[P * constants::FRAMES_IN_FLIGHT + _cycle.subIndex].get();
}

template<Renderer::Phase P> void Renderer::nextState() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    if constexpr(P >= PHASES_SIZE) {
        CTH_STABLE_ERR(P >= PHASES_SIZE, "invalid phase submitted") throw details->exception();
    } else _state = static_cast<Phase>(static_cast<size_t>(_state) + 1);
}



#ifdef CONSTANT_DEBUG_MODE
template<Renderer::Phase P> void Renderer::debug_check_current_phase(Renderer const* renderer) {
    DEBUG_CHECK_RENDERER_PHASE(P);
    CTH_ERR(P != renderer->_state, "phase error, explicitly const_skip phases") throw details->exception();
}



template<Renderer::Phase P>
constexpr void Renderer::debug_check_phase() {
    static_assert(P != PHASES_SIZE, "Phase P must not be PHASE_MAX_ENUM");
    static_assert(PHASES_FIRST == PHASE_TRANSFER && PHASES_LAST == PHASE_GRAPHICS && PHASE_TRANSFER == 0 && PHASE_GRAPHICS == 1,
        "enum values changed please adjust checks");
}


template<Renderer::Phase P>
void Renderer::debug_check_phase_change(Renderer const* renderer) {
    DEBUG_CHECK_RENDERER_PHASE(P);

    CTH_ERR(renderer->_state >= PHASES_SIZE, "phase error, forgot to call cycle?") throw details->exception();
    CTH_ERR(static_cast<size_t>(P) != static_cast<size_t>(renderer->_state), "phase error, explicitly const_skip phases") throw details->exception();

}
#endif

}


//Builder

namespace cth::vk {
template<Renderer::Phase P>
auto Renderer::Config::addSignalSets(std::span<BasicSemaphore* const> signal_semaphore_sets) -> Config& {
    add<P>(signal_semaphore_sets, _phaseSignalSets);

    return *this;
}
template<Renderer::Phase P>
auto Renderer::Config::addWaitSets(std::span<BasicSemaphore*> wait_semaphores, VkPipelineStageFlags wait_stage) -> Config& {
    std::vector<PipelineWaitStage> waitStages{};
    waitStages.reserve(wait_semaphores.size());

    std::ranges::transform(wait_semaphores, std::back_inserter(waitStages),
        [wait_stage](auto semaphore) { return PipelineWaitStage{wait_stage, semaphore}; });

    addWaitSets<P>(waitStages);

    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addWaitSets(std::span<PipelineWaitStage const> wait_stage_sets) -> Config& {
    add<P>(wait_stage_sets, _phaseWaitSets);

    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::removeSignalSets(std::span<BasicSemaphore* const> signal_semaphore_sets) -> Config& {

    remove<P>(signal_semaphore_sets, _phaseSignalSets);
    return *this;
}
template<Renderer::Phase P>
Renderer::Config& Renderer::Config::removeWaitSets(std::span<VkPipelineStageFlags const> wait_stage_sets) {
    remove<P>(wait_stage_sets, _phaseWaitSets);
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addQueue(Queue const* queue) -> Config& {
    DEBUG_CHECK_QUEUE(queue);
    DEBUG_CHECK_RENDERER_PHASE(P);

    _queues[P] = queue;
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::removeQueue(Queue const* queue) -> Config& {
    DEBUG_CHECK_QUEUE(queue);
    DEBUG_CHECK_RENDERER_PHASE(P);

    _queues[P] = nullptr;
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addPhase(Queue const* queue, std::optional<std::span<BasicSemaphore* const>> signal_semaphore_sets,
    std::optional<std::span<PipelineWaitStage const>> wait_stage_sets) -> Config& {
    addQueue<P>(queue);

    if(wait_stage_sets.has_value()) addSignalSets<P>(*wait_stage_sets);
    if(signal_semaphore_sets.has_value()) addWaitSets<P>(*signal_semaphore_sets);

    return *this;
}
template<Renderer::Phase P>
auto Renderer::Config::addSets(std::span<BasicSemaphore* const> signal_semaphore_sets,
    std::span<PipelineWaitStage const> wait_stage_sets) -> Config& {
    addSignalSets<P>(signal_semaphore_sets);
    addWaitSets<P>(wait_stage_sets);
    return *this;
}


template<Renderer::Phase P>
auto Renderer::Config::createPhaseSubmitInfos(
    std::span<PrimaryCmdBuffer const* const> phase_buffers) const -> std::vector<Queue::SubmitInfo> {

    DEBUG_CHECK_RENDERER_PHASE(P);

    auto const& signalSets = _phaseSignalSets[P];
    auto const& waitSets = _phaseWaitSets[P];


    std::vector<Queue::SubmitInfo> submitInfos{};
    submitInfos.reserve(SET_SIZE);

    for(size_t i = 0; i < SET_SIZE; i++) {
        auto extract = [i]<class Rng>(Rng const& rng) {
            using T = std::ranges::range_value_t<Rng>;
            return rng | std::views::drop(i) | std::views::stride(SET_SIZE)
                | std::ranges::to<std::vector<T>>();
        };

        auto signalSemaphores = extract(signalSets);
        auto waitStages = extract(waitSets);


        std::array cmdBuffers{phase_buffers[i]};
        submitInfos.emplace_back(cmdBuffers, waitStages, signalSemaphores, nullptr);
    }
    return submitInfos;
}

template<Renderer::Phase P, class T>
auto Renderer::Config::add(std::span<T const> sets, collection_t<T>& to) -> void {
    DEBUG_CHECK_RENDERER_PHASE(P);
    DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(sets);


    auto& phase = to[P];
    phase.reserve(phase.size() + sets.size());

    std::ranges::copy(sets, std::back_inserter(phase));
}

template<Renderer::Phase P, class T>
auto Renderer::Config::remove(std::span<T const> sets, collection_t<T>& from) -> void {
    DEBUG_CHECK_RENDERER_PHASE(P);
    DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(sets);

    auto& phase = from[P];
    for(auto& set : sets | std::views::chunk(SET_SIZE)) {
        auto const it = std::ranges::find_first_of(phase,
            [set](std::span<T const, SET_SIZE> phase_set) { return std::ranges::equal(set, phase_set); });

        std::erase(phase, it);
    }
}

#ifdef CONSTANT_DEBUG_MODE
template<class Rng>
void Renderer::Config::debug_check_sets_size(Rng const& rng) {
    CTH_ERR((std::ranges::size(rng) % SET_SIZE) != 0, "size must be a multiple of SET_SIZE") throw details->exception();
}
#endif


}
