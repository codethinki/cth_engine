#include "jolly/core/core.hpp"
#include "jolly/render/submit/queue.hpp"
#include "jolly/utility/GraphicsCore.hpp"

#include "jolly/utility/CthOSWindow.hpp"
#include "jolly/utility/GraphicsSyncConfig.hpp"
#include "jvk/base/core.hpp"
#include "jvk/surface/surface.hpp"
#include "jvk/surface/swapchain/swapchain.hpp"
#include "jvk/utility/format.hpp"
#include "jvk/utility/vk_exceptions.hpp"

#include "src/utility/vk_convert.hpp"


namespace jly {
GraphicsCore::GraphicsCore(jly::Core const& core, Config config) : _core{&core}, _config{std::move(config)} {}

GraphicsCore::GraphicsCore(jly::Core const& core, Config const& config, State state) :
    GraphicsCore{core, config} { wrap(std::move(state)); }

GraphicsCore::GraphicsCore(
    jly::Core const& core,
    Config const& config,
    std::string_view window_name,
    glm::uvec2 extent,
    Queue const& present_queue
) : GraphicsCore{core, config} { create(window_name, extent, present_queue); }

GraphicsCore::~GraphicsCore() { optDestroy(); }

void GraphicsCore::create(std::string_view window_name, glm::uvec2 extent, Queue const& present_queue) {
    optDestroy();

    auto& core = _core->raw();

    _osWindow = std::make_unique<OSWindow>(core.instance(), core.destructionQueue(), window_name, extent);
    _surface = std::make_unique<jvk::Surface>(
        core.instance(),
        core.destructionQueue(),
        jvk::Surface::Config{},
        jvk::Surface::State{_osWindow->releaseSurface()}
    );
    _syncConfig = std::make_unique<GraphicsSyncConfig>(core, framesInFlight());
    _swapchain = std::make_unique<jvk::Swapchain>(
        core,
        present_queue.raw(),
        *_surface,
        jvk::Swapchain::Config{
            .imageAvailableSemaphores{std::from_range, _syncConfig->imageAvailableSemaphores()},
            .renderFinishedSemaphores{std::from_range, _syncConfig->renderFinishedSemaphores()},
            .subpassConfig = _config.subpassConfig
        },
        to_vk_extent(osWindow()->framebufferExtent())
    );
}


void GraphicsCore::wrap(State state) {
    optDestroy();
    State::debug_check(state);

    _osWindow = state.osWindow.release_val();
    _surface = state.surface.release_val();
    _syncConfig = state.syncConfig.release_val();
    _swapchain = state.swapchain.release_val();
}

void GraphicsCore::destroy() {
    debug_check(*this);

    _swapchain = nullptr;
    _syncConfig = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
    reset();
}

auto GraphicsCore::release() -> State {
    debug_check(*this);

    State temp{
        std::move(_osWindow),
        std::move(_surface),
        std::move(_syncConfig),
        std::move(_swapchain)

    };
    reset();
    return temp;
}

void GraphicsCore::minimized() const {
    auto extent = _osWindow->framebufferExtent();

    if(extent.x != 0 && extent.y != 0) return;
    while(extent.x == 0 || extent.y == 0) {
        extent = _osWindow->framebufferExtent();
        _osWindow->waitEvents();
    }
}



void GraphicsCore::acquireFrame() {
    debug_check(*this);
    auto const result = _swapchain->acquireNextImage(_syncConfig->pulseVal());

    CTH_WARN(
        result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR,
        "swapchain image acquire result != VK_SUCCESS ({})",
        result
    ) {}

    _resize |= result != VK_SUCCESS;
}

void GraphicsCore::skipAcquire() const {
    debug_check(*this);
    _swapchain->skipAcquire(_syncConfig->pulseVal());
}


bool GraphicsCore::presentFrame() {
    debug_check(*this);
    auto const result = _swapchain->present(_syncConfig->pulseVal());

    _resize |= result != VK_SUCCESS;

    _syncConfig->next();

    return _resize;
}

void GraphicsCore::skipPresent() const {
    debug_check(*this);
    _swapchain->skipPresent(_syncConfig->pulseVal());
    _syncConfig->next();
}

void GraphicsCore::resize() {
    minimized();
    _swapchain->resize(to_vk_extent(_osWindow->framebufferExtent()));

    _resize = false;
}

void GraphicsCore::reset() {
    _swapchain = nullptr;
    _syncConfig = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
}



jvk::AttachmentCollection const* GraphicsCore::swapchainResolveAttachments() const {
    return _swapchain->resolveAttachments();
}

VkFormat GraphicsCore::swapchainImageFormat() const { return _swapchain->imageFormat(); }

glm::uvec2 GraphicsCore::swapchainExtent() const {
    auto const [width, height] = _swapchain->extent();
    return {width, height};
}

size_t GraphicsCore::swapchainSize() const { return _swapchain->size(); }

size_t GraphicsCore::swapchainImageIndex() const { return _swapchain->imageIndex(_syncConfig->pulseVal()); }

void GraphicsCore::State::debug_check(State const& state) {
    OSWindow::debug_check(state.osWindow.get());
    jvk::Surface::debug_check(*state.surface);
    jvk::Swapchain::debug_check(*state.swapchain.get());
}

}
