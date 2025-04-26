module cth.vk.present.core;

import cth.vk.constants;
import cth.vk.fmt;

namespace cth::vk {
PresentCore::PresentCore(Core const& core) : _core{&core} {}
PresentCore::PresentCore(Core const& core, State state) : PresentCore{core} { wrap(std::move(state)); }

PresentCore::PresentCore(Core const& core, std::string_view window_name, VkExtent2D extent,
    Queue const& present_queue) : PresentCore{core} { create(window_name, extent, present_queue); }

PresentCore::~PresentCore() { optDestroy(); }

void PresentCore::wrap(State state) {
    optDestroy();
    State::debug_check(state);

    _osWindow = state.osWindow.release_val();
    _surface = state.surface.release_val();
    _syncConfig = state.syncConfig.release_val();
    _swapchain = state.swapchain.release_val();
}


void PresentCore::create(std::string_view window_name, VkExtent2D extent, Queue const& present_queue) {
    optDestroy();


    _osWindow = std::make_unique<OSWindow>(_core->instance(), _core->destructionQueue(), window_name, extent);
    _surface = std::make_unique<Surface>(_core->instance(), _core->destructionQueue(), Surface::State{_osWindow->releaseSurface()});
    _syncConfig = std::make_unique<PresentSyncConfig>(*_core, vk::create);
    _swapchain = std::make_unique<BasicSwapchain>(*_core, present_queue, *_syncConfig, *_surface);
    _swapchain->create(_osWindow->extent()); //TEMP replace this with swapchain create constructor
}
void PresentCore::destroy() {
    debug_check(*this);

    _swapchain->destroy(); //TEMP replace this once non basic swapchain is ready
    _swapchain = nullptr;
    _syncConfig = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
    reset();
}
auto PresentCore::release() -> State {
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
void PresentCore::minimized() const {
    VkExtent2D extent = _osWindow->extent();

    if(extent.width != 0 && extent.height != 0) return;
    while(extent.width == 0 || extent.height == 0) {
        extent = _osWindow->extent();
        _osWindow->waitEvents();
    }
    _swapchain->resize(extent);

}



void PresentCore::acquireFrame() const {
    debug_check(*this);
    auto const result = _swapchain->acquireNextImage();

    CTH_WARN(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR, "swapchain image acquire result != VK_SUCCESS ({})", result) {}
}
void PresentCore::skipAcquire() const {
    debug_check(*this);
    _swapchain->skipAcquire();
}

void PresentCore::beginWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const {
    debug_check(*this);
    _swapchain->beginRenderPass(*render_cmd_buffer);
}
void PresentCore::endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const {
    debug_check(*this);
    _swapchain->endRenderPass(*render_cmd_buffer);
}


void PresentCore::presentFrame() const {
    debug_check(*this);
    auto const result = _swapchain->present();
    if(result != VK_SUCCESS) [[unlikely]] {
        minimized();
        _swapchain->resize(_osWindow->extent());
    }
    _syncConfig->next();
}
void PresentCore::skipPresent() const {
    debug_check(*this);
    _swapchain->skipPresent();
    _syncConfig->next();
}

void PresentCore::reset() {
    _swapchain = nullptr;
    _syncConfig = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
}

void PresentCore::State::debug_check(State const& state) {
    OSWindow::debug_check(state.osWindow.get());
    Surface::debug_check(*state.surface);
    BasicSwapchain::debug_check(state.swapchain.get());
}


RenderPass const* PresentCore::swapchainRenderPass() const { return _swapchain->renderPass(); }
VkSampleCountFlagBits PresentCore::msaaSamples() const { return _swapchain->msaaSamples(); }
} //namespace cth
