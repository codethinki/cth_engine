#include "CthGraphicsCore.hpp"

#include "vulkan/resource/CthDeletionQueue.hpp"

namespace cth {
GraphicsCore::GraphicsCore(const BasicCore* core, DeletionQueue* deletion_queue) : BasicGraphicsCore(core), _deletionQueue(deletion_queue) {
    DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue);
}
GraphicsCore::GraphicsCore(const BasicCore* core, DeletionQueue* deletion_queue, const std::string_view window_name, const VkExtent2D extent,
    const Queue* present_queue, const BasicGraphicsSyncConfig& sync_config) : GraphicsCore(core, deletion_queue) {
    GraphicsCore::create(window_name, extent, present_queue, sync_config);
}


GraphicsCore::~GraphicsCore() { optDestroy(); }
void GraphicsCore::wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) {
    optDestroy();
    BasicGraphicsCore::wrap(os_window, surface, swapchain);
}
void GraphicsCore::create(const std::string_view window_name, const VkExtent2D extent, const Queue* present_queue,
    const BasicGraphicsSyncConfig& sync_config, DeletionQueue* deletion_queue) {
    optDestroy();
    BasicGraphicsCore::create(window_name, extent, present_queue, sync_config, deletion_queue);
}
void GraphicsCore::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicGraphicsCore::destroy(_deletionQueue);
}
bool GraphicsCore::destroyed() const { return !osWindow() && !surface() && !swapchain(); }



} //namespace cth
