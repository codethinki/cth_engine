#include "CthGraphicsSyncConfig.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"


namespace cth::vk {

GraphicsSyncConfig::GraphicsSyncConfig(cth::not_null<Core const*> core, State state) : GraphicsSyncConfig{core} { wrap(std::move(state)); }
GraphicsSyncConfig::GraphicsSyncConfig(cth::not_null<Core const*> core, bool create) : GraphicsSyncConfig{core} { if(create) this->create(); }
void GraphicsSyncConfig::wrap(State state) {
    debug_check_state(state);
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

}
