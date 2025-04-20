#include "CthGraphicsCore.hpp"

#include "CthGraphicsSyncConfig.hpp"

#include "../CthOSWindow.hpp"
#include "../CthSurface.hpp"
#include "../swapchain/CthBasicSwapchain.hpp"
#include "src/vulkan/base/CthCore.hpp"
#include "src/vulkan/render/pass/CthRenderPass.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
GraphicsCore::GraphicsCore(Core const& core) : _core{&core} {}
GraphicsCore::GraphicsCore(Core const& core, State state) : GraphicsCore{core} { wrap(std::move(state)); }

GraphicsCore::GraphicsCore(Core const& core, std::string_view window_name, VkExtent2D extent,
    Queue const& present_queue) : GraphicsCore{core} { create(window_name, extent, present_queue); }

GraphicsCore::~GraphicsCore() { optDestroy(); }

void GraphicsCore::wrap(State state) {
    optDestroy();
    State::debug_check(state);

    _osWindow = state.osWindow.release_val();
    _surface = state.surface.release_val();
    _syncConfig = state.syncConfig.release_val();
    _swapchain = state.swapchain.release_val();
}


void GraphicsCore::create(std::string_view window_name, VkExtent2D extent, Queue const& present_queue) {
    optDestroy();


    _osWindow = std::make_unique<OSWindow>(_core->instance(), _core->destructionQueue(), window_name, extent);
    _surface = std::make_unique<Surface>(_core->instance(), _core->destructionQueue(), Surface::State{_osWindow->releaseSurface()});
    _syncConfig = std::make_unique<GraphicsSyncConfig>(*_core, vk::create);
    _swapchain = std::make_unique<BasicSwapchain>(*_core, present_queue, *_syncConfig, *_surface);
    _swapchain->create(_osWindow->extent()); //TEMP replace this with swapchain create constructor
}
void GraphicsCore::destroy() {
    debug_check(*this);

    _swapchain->destroy(); //TEMP replace this once non basic swapchain is ready
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
    VkExtent2D extent = _osWindow->extent();

    if(extent.width != 0 && extent.height != 0) return;
    while(extent.width == 0 || extent.height == 0) {
        extent = _osWindow->extent();
        _osWindow->waitEvents();
    }
    _swapchain->resize(extent);

}



void GraphicsCore::acquireFrame() const {
    debug_check(*this);
    auto const result = _swapchain->acquireNextImage();

    CTH_WARN(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR, "swapchain image acquire result != VK_SUCCESS ({})", result) {}
}
void GraphicsCore::skipAcquire() const {
    debug_check(*this);
    _swapchain->skipAcquire();
}

void GraphicsCore::beginWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const {
    debug_check(*this);
    _swapchain->beginRenderPass(*render_cmd_buffer);
}
void GraphicsCore::endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const {
    debug_check(*this);
    _swapchain->endRenderPass(*render_cmd_buffer);
}


void GraphicsCore::presentFrame() const {
    debug_check(*this);
    auto const result = _swapchain->present();
    if(result != VK_SUCCESS) [[unlikely]] {
        minimized();
        _swapchain->resize(_osWindow->extent());
    }
    _syncConfig->next();
}
void GraphicsCore::skipPresent() const {
    debug_check(*this);
    _swapchain->skipPresent();
    _syncConfig->next();
}

void GraphicsCore::reset() {
    _swapchain = nullptr;
    _syncConfig = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
}

void GraphicsCore::State::debug_check(State const& state) {
    OSWindow::debug_check(state.osWindow.get());
    Surface::debug_check(*state.surface);
    BasicSwapchain::debug_check(state.swapchain.get());
}


RenderPass const* GraphicsCore::swapchainRenderPass() const { return _swapchain->renderPass(); }
VkSampleCountFlagBits GraphicsCore::msaaSamples() const { return _swapchain->msaaSamples(); }
} //namespace cth
