#include "CthGraphicsSyncConfig.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"


namespace cth::vk {
#ifdef CONSTANT_DEBUG_MODE
void BasicGraphicsSyncConfig::debug_check_not_null(BasicGraphicsSyncConfig const* config) {
    CTH_ERR(config == nullptr, "sync config must not be nullptr")
        throw details->exception();
}
void BasicGraphicsSyncConfig::debug_check(BasicGraphicsSyncConfig const* config) {
    DEBUG_CHECK_SYNC_CONFIG_NOT_NULL(config);

    CTH_ERR(config->renderFinishedSemaphores.size() != constants::FRAMES_IN_FLIGHT
        || config->imageAvailableSemaphores.size() != constants::FRAMES_IN_FLIGHT,
        "one semaphore for each frame in flight required") {
        details->add("render finished semaphores: ({})", config->renderFinishedSemaphores.size());
        details->add("image available semaphores: ({})", config->imageAvailableSemaphores.size());
    }

    for(auto const& semaphore : config->renderFinishedSemaphores)
        DEBUG_CHECK_SEMAPHORE(semaphore);
    for(auto& semaphore : config->imageAvailableSemaphores)
        DEBUG_CHECK_SEMAPHORE(semaphore);
}
#endif
} //namespace cth

namespace cth::vk {


GraphicsSyncConfig::GraphicsSyncConfig(BasicCore const* core, DestructionQueue* destruction_queue) {
    create(core, destruction_queue);
}
GraphicsSyncConfig::~GraphicsSyncConfig() { destroyOpt(); }
void GraphicsSyncConfig::wrap(BasicGraphicsSyncConfig const& config) {
    DEBUG_CHECK_SYNC_CONFIG(&config);
    renderFinishedSemaphores = config.renderFinishedSemaphores;
    imageAvailableSemaphores = config.imageAvailableSemaphores;
}
void GraphicsSyncConfig::create(BasicCore const* core, DestructionQueue* destruction_queue) {
    destroyOpt();

    DEBUG_CHECK_CORE(core);
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue);
    _core = core;
    _destructionQueue = destruction_queue;

    renderFinishedSemaphores.reserve(constants::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++)
        renderFinishedSemaphores.emplace_back(new Semaphore(_core, _destructionQueue));

    imageAvailableSemaphores.reserve(constants::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < constants::FRAMES_IN_FLIGHT; i++)
        imageAvailableSemaphores.emplace_back(new Semaphore(_core, _destructionQueue));
}
void GraphicsSyncConfig::destroy(DestructionQueue* destruction_queue) {
    auto const config = release();

    for(auto const& semaphore : config.renderFinishedSemaphores) {
        semaphore->destroy(destruction_queue);
        delete semaphore;
    }

    for(auto const& semaphore : config.imageAvailableSemaphores) {
        semaphore->destroy(destruction_queue);
        delete semaphore;
    }
}
BasicGraphicsSyncConfig GraphicsSyncConfig::release() {
    BasicGraphicsSyncConfig const temp = {renderFinishedSemaphores, imageAvailableSemaphores};
    renderFinishedSemaphores.clear();
    imageAvailableSemaphores.clear();
    return temp;
}
bool GraphicsSyncConfig::destroyed() const { return renderFinishedSemaphores.empty() && imageAvailableSemaphores.empty(); }
void GraphicsSyncConfig::destroyOpt(DestructionQueue* destruction_queue) { if(!destroyed()) destroy(destruction_queue); }
} //namespace cth
