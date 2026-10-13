#include "jolly/utility/GraphicsSyncConfig.hpp"

#include "jolly/render/sync/pipeline_wait_stage.hpp"
#include <jvk/render/sync/semaphore.hpp>

#include <cth/algorithm/views.hpp>


namespace jly {

GraphicsSyncConfig::GraphicsSyncConfig(jvk::Core const& core) : _core{&core} {}

GraphicsSyncConfig::GraphicsSyncConfig(
    jvk::Core const& core,
    State state
) : GraphicsSyncConfig{core} { wrap(std::move(state)); }

GraphicsSyncConfig::GraphicsSyncConfig(jvk::Core const& core, size_t frames_in_flight) : GraphicsSyncConfig{core} {
    create(frames_in_flight);
}

GraphicsSyncConfig::~GraphicsSyncConfig() { optDestroy(); }

void GraphicsSyncConfig::wrap(State state) {
    State::debug_check(state);
    optDestroy();
    _pulse = std::move(state.pulse);
    _renderFinishedSemaphores = std::move(state.renderFinishedSemaphores);
    _imageAvailableSemaphores = std::move(state.imageAvailableSemaphores);
}

void GraphicsSyncConfig::create(size_t frames_in_flight) {
    optDestroy();
    _pulse.emplace(frames_in_flight);


    for(size_t i = 0; i < framesInFlight(); i++) {
        _renderFinishedSemaphores.emplace_back(*_core, jvk::create);
        _imageAvailableSemaphores.emplace_back(*_core, jvk::create);
    }
}

void GraphicsSyncConfig::destroy() {
    debug_check(*this);

    _renderFinishedSemaphores.clear();
    _imageAvailableSemaphores.clear();


    _pulse.reset();
}

GraphicsSyncConfig::State GraphicsSyncConfig::release() {
    debug_check(*this);

    return State{
        .pulse = *std::move(_pulse),
        .imageAvailableSemaphores = std::move(_imageAvailableSemaphores),
        .renderFinishedSemaphores = std::move(_renderFinishedSemaphores)
    };
}


//TEMP
//auto GraphicsSyncConfig::renderFinishedSemaphores() -> std::vector<jvk::Semaphore*> {
//    debug_check(*this);
//
//    return {std::from_range, _renderFinishedSemaphores | cth::views::to_ptr_range};
//}
//
//auto GraphicsSyncConfig::imageAvailableSemaphores() -> std::vector<jvk::Semaphore*> {
//    debug_check(*this);
//
//    return {std::from_range, _imageAvailableSemaphores | cth::views::to_ptr_range};
//}
std::vector<jvk::Semaphore const*> GraphicsSyncConfig::renderFinishedSemaphores() const {
    debug_check(*this);

    return {std::from_range, _renderFinishedSemaphores | cth::views::to_ptr_range};
}
std::vector<jvk::Semaphore const*> GraphicsSyncConfig::imageAvailableSemaphores() const {
    debug_check(*this);

    return {std::from_range, _imageAvailableSemaphores | cth::views::to_ptr_range};
}

auto GraphicsSyncConfig::imageAvailableWaitStages() const -> std::vector<PipelineWaitStage> {
    auto const semaphores = imageAvailableSemaphores();
    return {
        std::from_range,
        semaphores | std::views::transform(
            [](jvk::Semaphore const* ptr) { return PipelineWaitStage{PipelineStageFlags::TOP_OF_PIPE_BIT, ptr}; }
        )
    };
}

jvk::Semaphore const* GraphicsSyncConfig::renderFinishedSemaphore(size_t index) const {
    return &_renderFinishedSemaphores[index];
}
//jvk::Semaphore* GraphicsSyncConfig::renderFinishedSemaphore(size_t index) { return &_renderFinishedSemaphores[index]; }

jvk::Semaphore const* GraphicsSyncConfig::imageAvailableSemaphore(size_t index) const {
    return &_imageAvailableSemaphores[index];
}
//jvk::Semaphore* GraphicsSyncConfig::imageAvailableSemaphore(size_t index) { return &_imageAvailableSemaphores[index]; }

}


//State

namespace jly {
void GraphicsSyncConfig::State::debug_check(State const& state) {
    for(auto& semaphore : state.imageAvailableSemaphores)
        jvk::Semaphore::debug_check(semaphore);
    for(auto& semaphore : state.renderFinishedSemaphores)
        jvk::Semaphore::debug_check(semaphore);
}

}
