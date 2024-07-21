#include "CthGraphicsCore.hpp"

#include "vulkan/resource/CthDeletionQueue.hpp"

namespace cth::vk {
GraphicsCore::GraphicsCore(BasicCore const* core, DeletionQueue* deletion_queue) : BasicGraphicsCore(core), _deletionQueue(deletion_queue) {
    DEBUG_CHECK_DELETION_QUEUE_NULL_ALLOWED(deletion_queue);
}
GraphicsCore::GraphicsCore(BasicCore const* core, DeletionQueue* deletion_queue, std::string_view const window_name, VkExtent2D const extent,
    Queue const* present_queue, BasicGraphicsSyncConfig const& sync_config) : GraphicsCore(core, deletion_queue) {
    GraphicsCore::create(window_name, extent, present_queue, sync_config);
}


GraphicsCore::~GraphicsCore() { optDestroy(); }
void GraphicsCore::wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) {
    optDestroy();
    BasicGraphicsCore::wrap(os_window, surface, swapchain);
}
void GraphicsCore::create(std::string_view const window_name, VkExtent2D const extent, Queue const* present_queue,
    BasicGraphicsSyncConfig const& sync_config, DeletionQueue* deletion_queue) {
    optDestroy();
    BasicGraphicsCore::create(window_name, extent, present_queue, sync_config, deletion_queue);
}
void GraphicsCore::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) _deletionQueue = deletion_queue;
    BasicGraphicsCore::destroy(_deletionQueue);
}
bool GraphicsCore::destroyed() const { return !osWindow() && !surface() && !swapchain(); }



} //namespace cth
