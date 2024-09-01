#include "CthGraphicsCore.hpp"

#include "../CthOSWindow.hpp"
#include "../CthSurface.hpp"
#include "../swapchain/CthBasicSwapchain.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"

namespace cth::vk {
GraphicsCore::GraphicsCore(not_null<BasicCore const*> core) : _core{core} {}
GraphicsCore::GraphicsCore(not_null<BasicCore const*> core, State state) : GraphicsCore{core} { wrap(std::move(state)); }

GraphicsCore::GraphicsCore(not_null<BasicCore const*> core, std::string_view window_name, VkExtent2D extent,
    not_null<Queue const*> present_queue, not_null<BasicGraphicsSyncConfig const*> sync_config) : GraphicsCore{core} {
    create(window_name, extent, present_queue, sync_config);
}

GraphicsCore::~GraphicsCore() { optDestroy(); }

void GraphicsCore::wrap(State state) {
    optDestroy();
    DEBUG_CHECK_GRAPHICS_CORE_STATE(state);

    _swapchain = std::move(state.swapchain);
    _surface = std::move(state.surface);
    _osWindow = std::move(state.osWindow);
}


void GraphicsCore::create(std::string_view window_name, VkExtent2D extent, not_null<Queue const*> present_queue,
    not_null<BasicGraphicsSyncConfig const*> sync_config) {
    optDestroy();


    _osWindow = std::make_unique<OSWindow>(_core->instance(), _core->destructionQueue(), window_name, extent);
    _surface = std::make_unique<Surface>(_core->instance(), _core->destructionQueue(), _osWindow->releaseSurface());
    _swapchain = std::make_unique<BasicSwapchain>(_core, present_queue, sync_config, _surface.get());
    _swapchain->create(_osWindow->extent()); //TEMP replace this with swapchain create constructor
}
void GraphicsCore::destroy() {
    _swapchain->destroy(); //TEMP replace this once non basic swapchain is ready
    _swapchain = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
    reset();
}
auto GraphicsCore::release() -> State {
    DEBUG_CHECK_GRAPHICS_CORE(this);

    State temp{
        std::move(_osWindow),
        std::move(_surface),
        std::move(_swapchain)
    };
    reset();
    return temp;
}
void GraphicsCore::minimized() const {
    VkExtent2D extent = _osWindow->extent();
    while(extent.width == 0 || extent.height == 0) {
        extent = _osWindow->extent();
        _osWindow->waitEvents();
    }
    _swapchain->resize(extent);

}



void GraphicsCore::acquireFrame(Cycle const& cycle) const {
    DEBUG_CHECK_GRAPHICS_CORE(this);
    auto const result = _swapchain->acquireNextImage(cycle);

    CTH_WARN(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR, "suboptimal / out of date swapchain discovered on image acquire") {}
}
void GraphicsCore::skipAcquire(Cycle const& cycle) const {
    DEBUG_CHECK_GRAPHICS_CORE(this);
    _swapchain->skipAcquire(cycle);
}

void GraphicsCore::beginWindowPass(Cycle const& cycle, PrimaryCmdBuffer const* render_cmd_buffer) const {
    DEBUG_CHECK_GRAPHICS_CORE(this);
    _swapchain->beginRenderPass(cycle, render_cmd_buffer);
}
void GraphicsCore::endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const {
    DEBUG_CHECK_GRAPHICS_CORE(this);
    _swapchain->endRenderPass(render_cmd_buffer);
}


void GraphicsCore::presentFrame(Cycle const& cycle) const {
    DEBUG_CHECK_GRAPHICS_CORE(this);
    auto const result = _swapchain->present(cycle);
    if(result != VK_SUCCESS) [[unlikely]]
        _swapchain->resize(_osWindow->extent());
}
void GraphicsCore::skipPresent(Cycle const& cycle) const {
    DEBUG_CHECK_GRAPHICS_CORE(this);
    _swapchain->skipPresent(cycle);
}

void GraphicsCore::reset() {
    _osWindow = nullptr;
    _surface = nullptr;
    _swapchain = nullptr;
}


RenderPass const* GraphicsCore::swapchainRenderPass() const { return _swapchain->renderPass(); }
VkSampleCountFlagBits GraphicsCore::msaaSamples() const { return _swapchain->msaaSamples(); }


#ifdef CONSTANT_DEBUG_MODE
void GraphicsCore::debug_check(not_null<GraphicsCore const*> graphics_core) {
    CTH_ERR(!graphics_core->created(), "graphics core must be created") throw details->exception();
}
void GraphicsCore::debug_check_state(State const& state) {
    DEBUG_CHECK_OS_WINDOW(state.osWindow.get());
    DEBUG_CHECK_SURFACE(state.surface.get());
    DEBUG_CHECK_SWAPCHAIN(state.swapchain.get());
}
#endif

} //namespace cth
