#include "CthGraphicsSyncConfig.hpp"

#include <cth/cth_log.hpp>

#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"


namespace cth {
#ifdef CONSTANT_DEBUG_MODE
void BasicGraphicsSyncConfig::debug_check_not_null(const BasicGraphicsSyncConfig* config) {
    CTH_ERR(config == nullptr, "sync config must not be nullptr")
        throw details->exception();
}
void BasicGraphicsSyncConfig::debug_check(const BasicGraphicsSyncConfig* config) {
    DEBUG_CHECK_SYNC_CONFIG_NOT_NULL(config);

    CTH_ERR(config->renderFinishedSemaphores.size() != Constant::FRAMES_IN_FLIGHT
        || config->imageAvailableSemaphores.size() != Constant::FRAMES_IN_FLIGHT,
        "one semaphore for each frame in flight required") {
        details->add("render finished semaphores: ({})", config->renderFinishedSemaphores.size());
        details->add("image available semaphores: ({})", config->imageAvailableSemaphores.size());
    }

    for(const auto& semaphore : config->renderFinishedSemaphores)
        DEBUG_CHECK_SEMAPHORE(semaphore);
    for(auto& semaphore : config->imageAvailableSemaphores)
        DEBUG_CHECK_SEMAPHORE(semaphore);
}
#endif
} //namespace cth

namespace cth {


GraphicsSyncConfig::GraphicsSyncConfig(const BasicCore* core, DeletionQueue* deletion_queue) {
    create(core, deletion_queue);
}
GraphicsSyncConfig::~GraphicsSyncConfig() { destroyOpt(); }
void GraphicsSyncConfig::wrap(const BasicGraphicsSyncConfig& config) {
    DEBUG_CHECK_SYNC_CONFIG(&config);
    renderFinishedSemaphores = config.renderFinishedSemaphores;
    imageAvailableSemaphores = config.imageAvailableSemaphores;
}
void GraphicsSyncConfig::create(const BasicCore* core, DeletionQueue* deletion_queue) {
    destroyOpt();

    DEBUG_CHECK_CORE(core);
    DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue);
    _core = core;
    _deletionQueue = deletion_queue;

    renderFinishedSemaphores.reserve(Constant::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < Constant::FRAMES_IN_FLIGHT; i++)
        renderFinishedSemaphores.emplace_back(new Semaphore(_core, _deletionQueue));

    imageAvailableSemaphores.reserve(Constant::FRAMES_IN_FLIGHT);
    for(size_t i = 0; i < Constant::FRAMES_IN_FLIGHT; i++)
        imageAvailableSemaphores.emplace_back(new Semaphore(_core, _deletionQueue));
}
void GraphicsSyncConfig::destroy(DeletionQueue* deletion_queue) {
    auto config = release();

    for(auto& semaphore : config.renderFinishedSemaphores){
        semaphore->destroy(deletion_queue);
        delete semaphore;
    }

    for(auto& semaphore : config.imageAvailableSemaphores){
        semaphore->destroy(deletion_queue);
        delete semaphore;
    }
}
BasicGraphicsSyncConfig GraphicsSyncConfig::release() {
    const BasicGraphicsSyncConfig temp = {renderFinishedSemaphores, imageAvailableSemaphores};
    renderFinishedSemaphores.clear();
    imageAvailableSemaphores.clear();
    return temp;
}
bool GraphicsSyncConfig::destroyed() const { return renderFinishedSemaphores.empty() && imageAvailableSemaphores.empty(); }
void GraphicsSyncConfig::destroyOpt(DeletionQueue* deletion_queue) { if(!destroyed()) destroy(deletion_queue); }
} //namespace cth
