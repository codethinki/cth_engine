#include "CthGraphicsSyncConfig.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"


namespace cth::vk {

GraphicsSyncConfig::GraphicsSyncConfig(not_null<BasicCore const*> core, State state) : GraphicsSyncConfig{core} {
    wrap(std::move(state));
}
GraphicsSyncConfig::GraphicsSyncConfig(not_null<BasicCore const*> core, bool create) : GraphicsSyncConfig{core} {
    if(create) this->create();
}
void GraphicsSyncConfig::wrap(State state) {
    DEBUG_CHECK_GRAPHICS_SYNC_CONFIG_STATE(state);
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
    DEBUG_CHECK_GRAPHICS_SYNC_CONFIG(this);

    for(auto& semaphore : _renderFinishedSemaphores) semaphore = nullptr;
    for(auto& semaphore : _imageAvailableSemaphores) semaphore = nullptr;
}
GraphicsSyncConfig::State GraphicsSyncConfig::release() {
    DEBUG_CHECK_GRAPHICS_SYNC_CONFIG(this);

    return State{
        .imageAvailableSemaphores = std::move(_imageAvailableSemaphores),
        .renderFinishedSemaphores = std::move(_renderFinishedSemaphores)
    };
}
std::array<BasicSemaphore*, GraphicsSyncConfig::SET_SIZE> GraphicsSyncConfig::renderFinishedSemaphores() const {
    DEBUG_CHECK_GRAPHICS_SYNC_CONFIG(this);


    std::array<BasicSemaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_renderFinishedSemaphores, semaphores)) dst = src.get();
    return semaphores;
}
std::array<BasicSemaphore*, GraphicsSyncConfig::SET_SIZE> GraphicsSyncConfig::imageAvailableSemaphores() const {
    DEBUG_CHECK_GRAPHICS_SYNC_CONFIG(this);

    std::array<BasicSemaphore*, SET_SIZE> semaphores{};
    for(auto [src, dst] : std::views::zip(_imageAvailableSemaphores, semaphores)) dst = src.get();
    return semaphores;
}



#ifdef CONSTANT_DEBUG_MODE
void GraphicsSyncConfig::debug_check(not_null<GraphicsSyncConfig const*> config) {
    CTH_ERR(!config->created(), "config not created") throw details->exception();
}
void GraphicsSyncConfig::debug_check_state(State const& state) {
    for(auto& semaphore : state.imageAvailableSemaphores)
        DEBUG_CHECK_SEMAPHORE(semaphore.get());
    for(auto& semaphore : state.renderFinishedSemaphores)
        DEBUG_CHECK_SEMAPHORE(semaphore.get());
}
#endif
} //namespace cth
