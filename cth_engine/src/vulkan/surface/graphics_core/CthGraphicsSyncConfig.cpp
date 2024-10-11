#include "CthGraphicsSyncConfig.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"


namespace cth::vk {

GraphicsSyncConfig::GraphicsSyncConfig(cth::not_null<Core const*> core) : _core{core} {}
GraphicsSyncConfig::GraphicsSyncConfig(cth::not_null<Core const*> core, State state) : GraphicsSyncConfig{core} { wrap(std::move(state)); }
GraphicsSyncConfig::GraphicsSyncConfig(cth::not_null<Core const*> core, bool create) : GraphicsSyncConfig{core} { if(create) this->create(); }
GraphicsSyncConfig::~GraphicsSyncConfig() { optDestroy(); }
void GraphicsSyncConfig::wrap(State state) {
    State::debug_check(state);
    optDestroy();

    _renderFinishedSemaphores = std::move(state.renderFinishedSemaphores);
    _imageAvailableSemaphores = std::move(state.imageAvailableSemaphores);
}
void GraphicsSyncConfig::create() {
    optDestroy();

    for(auto& uniquePtr : _renderFinishedSemaphores)
        uniquePtr = std::make_unique<Semaphore>(_core, true);
    for(auto& uniquePtr : _imageAvailableSemaphores)
        uniquePtr = std::make_unique<Semaphore>(_core, true);
}
void GraphicsSyncConfig::destroy() {
    debug_check(this);

    for(auto& semaphore : _renderFinishedSemaphores) semaphore = nullptr;
    for(auto& semaphore : _imageAvailableSemaphores) semaphore = nullptr;
}
GraphicsSyncConfig::State GraphicsSyncConfig::release() {
    debug_check(this);

    return State{
        .imageAvailableSemaphores = std::move(_imageAvailableSemaphores),
        .renderFinishedSemaphores = std::move(_renderFinishedSemaphores)
    };
}
std::array<Semaphore*, GraphicsSyncConfig::SET_SIZE> GraphicsSyncConfig::renderFinishedSemaphores() const {
    debug_check(this);


    std::array<Semaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_renderFinishedSemaphores, semaphores)) dst = src.get();
    return semaphores;
}
std::array<Semaphore*, GraphicsSyncConfig::SET_SIZE> GraphicsSyncConfig::imageAvailableSemaphores() const {
    debug_check(this);

    std::array<Semaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_imageAvailableSemaphores, semaphores)) dst = src.get();
    return semaphores;
}
Semaphore* GraphicsSyncConfig::renderFinishedSemaphore(size_t index) const { return _renderFinishedSemaphores[index].get(); }
Semaphore* GraphicsSyncConfig::imageAvailableSemaphore(size_t index) const { return _imageAvailableSemaphores[index].get(); }

}


//State

namespace cth::vk {
void GraphicsSyncConfig::State::debug_check(State const& state) {
    for(auto& semaphore : state.imageAvailableSemaphores)
        Semaphore::debug_check(semaphore.get());
    for(auto& semaphore : state.renderFinishedSemaphores)
        Semaphore::debug_check(semaphore.get());
}

}
