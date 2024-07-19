#pragma once

#include "vulkan/base/CthQueue.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/io/cth_log.hpp>

#include <ranges>
#include <vector>

namespace cth::vk {
template<Renderer::Phase P>
PrimaryCmdBuffer* Renderer::begin() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    if constexpr(P == PHASES_FIRST) wait();

    auto buffer = cmdBuffer<P>();
    const VkResult beginResult = buffer->begin();
    CTH_STABLE_ERR(beginResult != VK_SUCCESS, "failed to begin command buffer")
        throw except::vk_result_exception{beginResult, details->exception()};

    return buffer;
}
template<Renderer::Phase P>
void Renderer::end() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    const PrimaryCmdBuffer* buffer = cmdBuffer<P>();

    const VkResult recordResult = buffer->end();
    CTH_STABLE_ERR(recordResult != VK_SUCCESS, "failed to record command buffer")
        throw except::vk_result_exception{recordResult, details->exception()};


    submit<P>();
}
template<Renderer::Phase P> void Renderer::skip() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);



    nextState<P>();
}

template<Renderer::Phase P>
void Renderer::submit() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    queue<P>()->submit(submitInfo<P>());
    nextState<P>();
}



template<Renderer::Phase P>
const Queue* Renderer::queue() const { return _queues[P]; }
template<Renderer::Phase P>
PrimaryCmdBuffer* Renderer::cmdBuffer() const {
    DEBUG_CHECK_RENDERER_CURRENT_PHASE(this, P);
    return _cmdBuffers[_frameIndex * PHASES_SIZE + P].get();
}

template<Renderer::Phase P> void Renderer::nextState() {
    DEBUG_CHECK_RENDERER_PHASE_CHANGE(this, P);

    if constexpr(P == PHASES_LAST) _state = PHASES_FIRST;
    else _state = static_cast<Phase>(static_cast<size_t>(_state) + 1);

}



#ifdef CONSTANT_DEBUG_MODE
template<Renderer::Phase P> void Renderer::debug_check_current_phase(const Renderer* renderer) {
    DEBUG_CHECK_RENDERER_PHASE(P);
    CTH_ERR(P != renderer->_state, "phase error, explicitly skip phases") throw details->exception();
}



template<Renderer::Phase P>
constexpr void Renderer::debug_check_phase() {
    static_assert(P != PHASES_SIZE, "Phase P must not be PHASE_MAX_ENUM");
    static_assert(PHASES_FIRST == PHASE_TRANSFER && PHASES_LAST == PHASE_GRAPHICS && PHASE_TRANSFER == 0 && PHASE_GRAPHICS == 1,
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

namespace cth::vk {
template<Renderer::Phase P>
auto Renderer::Config::addSignalSets(const std::span<BasicSemaphore* const> signal_semaphore_sets) -> Config& {
    add<P>(signal_semaphore_sets, _phaseSignalSets);

    return *this;
}
template<Renderer::Phase P>
auto Renderer::Config::addWaitSets(std::span<BasicSemaphore*> wait_semaphores, VkPipelineStageFlags wait_stage) -> Config& {
    std::vector<PipelineWaitStage> waitStages{};
    waitStages.reserve(wait_semaphores.size());

    std::ranges::transform(wait_semaphores, std::back_inserter(waitStages),
        [wait_stage](const auto semaphore) { return PipelineWaitStage{wait_stage, semaphore}; });

    addWaitSets<P>(waitStages);

    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addWaitSets(const std::span<const PipelineWaitStage> wait_stage_sets) -> Config& {
    add<P>(wait_stage_sets, _phaseWaitSets);

    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::removeSignalSets(const std::span<BasicSemaphore* const> signal_semaphore_sets) -> Config& {

    remove<P>(signal_semaphore_sets, _phaseSignalSets);
    return *this;
}
template<Renderer::Phase P>
Renderer::Config& Renderer::Config::removeWaitSets(std::span<const VkPipelineStageFlags> wait_stage_sets) {
    remove<P>(wait_stage_sets, _phaseWaitSets);
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addQueue(const Queue* queue) -> Config& {
    DEBUG_CHECK_QUEUE(queue);
    DEBUG_CHECK_RENDERER_PHASE(P);

    _queues[P] = queue;
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::removeQueue(const Queue* queue) -> Config& {
    DEBUG_CHECK_QUEUE(queue);
    DEBUG_CHECK_RENDERER_PHASE(P);

    _queues[P] = nullptr;
    return *this;
}

template<Renderer::Phase P>
auto Renderer::Config::addPhase(const Queue* queue, std::optional<std::span<BasicSemaphore* const>> signal_semaphore_sets,
    std::optional<std::span<const PipelineWaitStage>> wait_stage_sets) -> Config& {
    addQueue<P>(queue);

    if(wait_stage_sets.has_value()) addSignalSets<P>(*wait_stage_sets);
    if(signal_semaphore_sets.has_value()) addWaitSets<P>(*signal_semaphore_sets);

    return *this;
}
template<Renderer::Phase P>
auto Renderer::Config::addSets(const std::span<BasicSemaphore* const> signal_semaphore_sets,
    const std::span<const PipelineWaitStage> wait_stage_sets) -> Config& {
    addSignalSets<P>(signal_semaphore_sets);
    addWaitSets<P>(wait_stage_sets);
    return *this;
}


template<Renderer::Phase P>
auto Renderer::Config::createPhaseSubmitInfos(
    const std::span<const PrimaryCmdBuffer* const> phase_buffers) const -> std::vector<Queue::SubmitInfo> {

    DEBUG_CHECK_RENDERER_PHASE(P);

    const auto& signalSets = _phaseSignalSets[P];
    const auto& waitSets = _phaseWaitSets[P];


    std::vector<Queue::SubmitInfo> submitInfos{};
    submitInfos.reserve(SET_SIZE);

    for(auto [index, info] : submitInfos | std::views::enumerate) {
        const auto i = static_cast<size_t>(index);

        auto extract = [i]<class Rng>(const Rng& rng) {
            using T = type::range2d_value_t<Rng>;
            return rng | std::views::transform([i](const auto& set) { return set[i]; })
                | std::ranges::to<std::vector<T>>();
        };

        auto semaphores = extract(signalSets);
        auto waitStages = extract(waitSets);

        submitInfos[i] = Queue::SubmitInfo{phase_buffers, waitStages, semaphores, nullptr};
    }
    return submitInfos;
}

template<Renderer::Phase P, class T>
auto Renderer::Config::add(std::span<T const> sets, collection_t<T>& to) -> void {
    DEBUG_CHECK_RENDERER_PHASE(P);
    DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(sets);


    auto& phase = to[P];
    for(const auto& set : sets | std::views::chunk(SET_SIZE)) {
        phase.emplace_back();

        std::ranges::copy(set, phase.back().begin());
    }
}

template<Renderer::Phase P, class T>
auto Renderer::Config::remove(std::span<T const> sets, collection_t<T>& from) -> void {
    DEBUG_CHECK_RENDERER_PHASE(P);
    DEBUG_CHECK_RENDERER_CONFIG_SET_SIZE(sets);

    auto& phase = from[P];
    for(auto& set : sets | std::views::chunk(SET_SIZE)) {
        const auto it = std::ranges::find_first_of(phase,
            [set](std::span<T const, SET_SIZE> phase_set) { return std::ranges::equal(set, phase_set); });

        std::erase(phase, it);
    }
}

#ifdef CONSTANT_DEBUG_MODE
template<class Rng>
void Renderer::Config::debug_check_sets_size(const Rng& rng) {
    CTH_ERR((std::ranges::size(rng) % SET_SIZE) != 0, "size must be a multiple of SET_SIZE") throw details->exception();
}
#endif


}
