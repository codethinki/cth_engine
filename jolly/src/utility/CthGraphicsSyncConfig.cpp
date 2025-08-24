#include "jolly/utility/GraphicsSyncConfig.hpp"

#include "jvk/base/core.hpp"
#include "jvk/render/ctrl/semaphore.hpp"
#include "jvk/res/destruction_queue.hpp"


namespace jly {

GraphicsSyncConfig::GraphicsSyncConfig(jvk::Core const& core) : _core{&core} {}

GraphicsSyncConfig::GraphicsSyncConfig(jvk::Core const& core, State state) : GraphicsSyncConfig{core} {
    wrap(std::move(state));
}

GraphicsSyncConfig::GraphicsSyncConfig(jvk::Core const& core, create_t) : GraphicsSyncConfig{core} {
    create();
}

GraphicsSyncConfig::~GraphicsSyncConfig() { optDestroy(); }

void GraphicsSyncConfig::wrap(State state) {
    State::debug_check(state);
    optDestroy();
    _pulse = std::move(state.pulse);
    _renderFinishedSemaphores = std::move(state.renderFinishedSemaphores);
    _imageAvailableSemaphores = std::move(state.imageAvailableSemaphores);
}

void GraphicsSyncConfig::create() {
    optDestroy();

    for(auto& uniquePtr : _renderFinishedSemaphores) //TODO refactor this to init and make create only create
        uniquePtr = std::make_unique<jvk::Semaphore>(*_core, jvk::create);
    for(auto& uniquePtr : _imageAvailableSemaphores)
        uniquePtr = std::make_unique<jvk::Semaphore>(*_core, jvk::create);
}

void GraphicsSyncConfig::destroy() {
    debug_check(*this);

    for(auto& semaphore : _renderFinishedSemaphores) semaphore = nullptr; //TODO same as create()
    for(auto& semaphore : _imageAvailableSemaphores) semaphore = nullptr;


    _pulse.reset();
}

GraphicsSyncConfig::State GraphicsSyncConfig::release() {
    debug_check(*this);

    return State{
        .imageAvailableSemaphores = std::move(_imageAvailableSemaphores),
        .renderFinishedSemaphores = std::move(_renderFinishedSemaphores)
    };
}



auto GraphicsSyncConfig::renderFinishedSemaphores() const -> std::array<jvk::Semaphore*, SET_SIZE> {
    debug_check(*this);


    std::array<jvk::Semaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_renderFinishedSemaphores, semaphores)) dst = src.get();
    return semaphores;
}

auto GraphicsSyncConfig::imageAvailableSemaphores() const -> std::array<jvk::Semaphore*, SET_SIZE> {
    debug_check(*this);

    std::array<jvk::Semaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_imageAvailableSemaphores, semaphores)) dst = src.get();
    return semaphores;
}

auto GraphicsSyncConfig::imageAvailableWaitStages() const -> std::vector<jvk::PipelineWaitStage> {
    auto const semaphores = imageAvailableSemaphores();
    return {
        std::from_range,
        semaphores | std::views::transform([](jvk::Semaphore const* ptr) {
            return jvk::PipelineWaitStage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ptr};
        })
    };


}

jvk::Semaphore* GraphicsSyncConfig::renderFinishedSemaphore(size_t index) const {
    return _renderFinishedSemaphores[index].get();
}

jvk::Semaphore* GraphicsSyncConfig::imageAvailableSemaphore(size_t index) const {
    return _imageAvailableSemaphores[index].get();
}

}


//State

namespace jly {
void GraphicsSyncConfig::State::debug_check(State const& state) {
    for(auto& semaphore : state.imageAvailableSemaphores)
        jvk::Semaphore::debug_check(*semaphore);
    for(auto& semaphore : state.renderFinishedSemaphores)
        jvk::Semaphore::debug_check(*semaphore);
}

}
