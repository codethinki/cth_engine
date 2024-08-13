#include "CthGraphicsCore.hpp"

#include "vulkan/resource/CthDestructionQueue.hpp"

namespace cth::vk {
GraphicsCore::GraphicsCore(BasicCore const* core, DestructionQueue* destruction_queue) : BasicGraphicsCore(core), _destructionQueue(destruction_queue) {
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue);
}
GraphicsCore::GraphicsCore(BasicCore const* core, DestructionQueue* destruction_queue, std::string_view const window_name, VkExtent2D const extent,
    Queue const* present_queue, BasicGraphicsSyncConfig const& sync_config) : GraphicsCore(core, destruction_queue) {
    GraphicsCore::create(window_name, extent, present_queue, &sync_config);
}


GraphicsCore::~GraphicsCore() { optDestroy(); }
void GraphicsCore::wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) {
    optDestroy();
    BasicGraphicsCore::wrap(os_window, surface, swapchain);
}
void GraphicsCore::create(std::string_view const window_name, VkExtent2D const extent, Queue const* present_queue,
    BasicGraphicsSyncConfig const* sync_config, DestructionQueue* destruction_queue) {
    optDestroy();
    BasicGraphicsCore::create(window_name, extent, present_queue, sync_config, destruction_queue);
}
void GraphicsCore::destroy(DestructionQueue* destruction_queue) {
    if(destruction_queue) _destructionQueue = destruction_queue;
    BasicGraphicsCore::destroy(_destructionQueue);
}
bool GraphicsCore::destroyed() const { return !osWindow() && !surface() && !swapchain(); }



} //namespace cth
