#include "CthBasicGraphicsCore.hpp"

#include "../CthOSWindow.hpp"
#include "../CthSurface.hpp"
#include "../swapchain/CthBasicSwapchain.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"

namespace cth::vk {
BasicGraphicsCore::BasicGraphicsCore(BasicCore const* core, OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) : _core(core) {
    BasicGraphicsCore::wrap(os_window, surface, swapchain);
}
#ifdef CONSTANT_DEBUG_MODE
BasicGraphicsCore::~BasicGraphicsCore() { DEBUG_CHECK_GRAPHICS_CORE_LEAK(this); }
#endif

void BasicGraphicsCore::wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) {
    DEBUG_CHECK_OS_WINDOW(os_window);
    DEBUG_CHECK_SURFACE(surface);
    DEBUG_CHECK_SWAPCHAIN(swapchain);

    _osWindow = os_window;
    _surface = surface;
    _swapchain = swapchain;
}


void BasicGraphicsCore::create(std::string_view const window_name, VkExtent2D const extent, Queue const* present_queue,
    BasicGraphicsSyncConfig const* sync_config, DestructionQueue* destruction_queue) {
    DEBUG_CHECK_GRAPHICS_CORE_LEAK(this);
    _osWindow = new OSWindow(_core->instance(), destruction_queue, window_name, extent.width, extent.height);
    _surface = new Surface(_core->instance(), destruction_queue, _osWindow->releaseSurface());
    _swapchain = new BasicSwapchain(_core, destruction_queue, present_queue, sync_config);
    _swapchain->create(_surface.get(), _osWindow->extent());
}
void BasicGraphicsCore::destroy(DestructionQueue* destruction_queue) {
    _swapchain->destroy(destruction_queue);
    _surface->destroy(destruction_queue);
    _osWindow->destroy(destruction_queue);
    auto const ptrs = release();
    delete ptrs.swapchain; //TEMP left off here the swapchain gets submitted to the deletion queue while the surface and window do not -> no proper destruction
    delete ptrs.surface;
    delete ptrs.osWindow;
}
auto BasicGraphicsCore::release() -> handles {

    auto const temp = handles{_osWindow.get(), _surface.get(), _swapchain.get()};


    _swapchain = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;

    return temp;
}
void BasicGraphicsCore::minimized() const {
    VkExtent2D extent = _osWindow->extent();
    while(extent.width == 0 || extent.height == 0) {
        extent = _osWindow->extent();
        _osWindow->waitEvents();
    }
    _swapchain->resize(extent);

}



void BasicGraphicsCore::acquireFrame(DestructionQueue* destruction_queue) const {
    auto const result = _swapchain->acquireNextImage(destruction_queue);

    CTH_WARN(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR, "suboptimal / out of date swapchain discovered on image acquire") {}
}

void BasicGraphicsCore::beginWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const { _swapchain->beginRenderPass(render_cmd_buffer); }
void BasicGraphicsCore::endWindowPass(PrimaryCmdBuffer const* render_cmd_buffer) const { _swapchain->endRenderPass(render_cmd_buffer); }


void BasicGraphicsCore::presentFrame() const {
    auto const result = _swapchain->present();
    if(result != VK_SUCCESS) [[unlikely]]
        _swapchain->resize(_osWindow->extent());
}



#ifdef CONSTANT_DEBUG_MODE
void BasicGraphicsCore::debug_check_not_null(BasicGraphicsCore const* graphics_core) {
    CTH_ERR(!graphics_core, "graphics core must not be nullptr")
        throw details->exception();
}
void BasicGraphicsCore::debug_check_leak(BasicGraphicsCore const* graphics_core) {
    DEBUG_CHECK_GRAPHICS_CORE_NOT_NULL(graphics_core);

    CTH_WARN(graphics_core->_osWindow != nullptr, "osWindow not nullptr (potential memory leak)") {}
    CTH_WARN(graphics_core->_surface != nullptr, "_surface not nullptr (potential memory leak)") {}
    CTH_WARN(graphics_core->_swapchain != nullptr, "swapchain not nullptr (potential memory leak)") {}
}
#endif

} //namespace cth
