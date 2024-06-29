#include "CthBasicGraphicsCore.hpp"

#include "../CthOSWindow.hpp"
#include "../CthSurface.hpp"
#include "../swapchain/CthBasicSwapchain.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

namespace cth {
BasicGraphicsCore::BasicGraphicsCore(const BasicCore* core, OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) : _core(core), _osWindow(os_window), _surface(surface),
    _swapchain(swapchain) {
    DEBUG_CHECK_CORE(core);
    DEBUG_CHECK_SWAPCHAIN(swapchain);
    DEBUG_CHECK_SURFACE(surface);


}

void BasicGraphicsCore::create(const string_view window_name, const uint32_t width, const uint32_t height, const Queue* present_queue, const GraphicsSyncConfig& sync_config) {
    DEBUG_CHECK_GRAPHICS_CORE_LEAK(this);
    
    _osWindow = new OSWindow(window_name, width, height, _core->instance());
    _surface = new Surface(_osWindow->surface()->get(), _core->instance());
    _swapchain = new BasicSwapchain(_core, present_queue, sync_config);
}
void BasicGraphicsCore::destroy(DeletionQueue* deletion_queue) {
    _swapchain->destroy(deletion_queue);
    delete _swapchain;

    delete _surface;

    delete _osWindow;

    reset();
}
void BasicGraphicsCore::reset() {
    DEBUG_CHECK_GRAPHICS_CORE_LEAK(this);

    _swapchain = nullptr;
    _surface = nullptr;
    _osWindow = nullptr;
}


void BasicGraphicsCore::acquireFrame() const {
    const auto result = _swapchain->acquireNextImage();
    if(result == VK_SUCCESS) return;

    static_assert(false, "implement: this class in a non basic variant"); //IMPLEMENT this class in a non basic variant
    

    _swapchain->resize(_osWindow->extent());
    const auto result2 = _swapchain->acquireNextImage();
    CTH_ERR(result2 != VK_SUCCESS, "failed to acquire next image")
        throw cth::except::vk_result_exception{result2, details->exception()};
}

void BasicGraphicsCore::beginWindowPass(const PrimaryCmdBuffer* render_cmd_buffer) const { _swapchain->beginRenderPass(render_cmd_buffer); }
void BasicGraphicsCore::endWindowPass(const PrimaryCmdBuffer* render_cmd_buffer) const { _swapchain->endRenderPass(render_cmd_buffer); }


void BasicGraphicsCore::presentFrame(DeletionQueue* deletion_queue) const {
    const auto result = _swapchain->present(deletion_queue);
    if(result == VK_SUBOPTIMAL_KHR) _swapchain->resize(_osWindow->extent());
}



#ifdef CONSTANT_DEBUG_MODE
void BasicGraphicsCore::debug_check_not_null(const BasicGraphicsCore* graphics_core) {
    CTH_ERR(!graphics_core, "graphics core must not be nullptr")
        throw details->exception();
}
void BasicGraphicsCore::debug_check_leak(const BasicGraphicsCore* graphics_core) {
    DEBUG_CHECK_GRAPHICS_CORE_NOT_NULL(graphics_core);

    CTH_WARN(graphics_core->_osWindow != nullptr, "osWindow not nullptr (potential memory leak)");
    CTH_WARN(graphics_core->_surface != nullptr, "_surface not nullptr (potential memory leak)");
    CTH_WARN(graphics_core->_swapchain != nullptr, "swapchain not nullptr (potential memory leak)");
}
#endif

} //namespace cth
