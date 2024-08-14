#include "CthGraphicsCore.hpp"

#include "CthBasicGraphicsCore.hpp"

#include "vulkan/resource/CthDestructionQueue.hpp"

namespace cth::vk {
GraphicsCore::GraphicsCore(BasicCore const* core) : BasicGraphicsCore(core) {}
GraphicsCore::GraphicsCore(BasicCore const* core, std::string_view const window_name, VkExtent2D const extent,
    Queue const* present_queue, BasicGraphicsSyncConfig const& sync_config) : GraphicsCore(core) {
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
    if(destruction_queue) BasicGraphicsCore::destroy(destruction_queue);
    else BasicGraphicsCore::destroy(_core->destructionQueue());
}
bool GraphicsCore::destroyed() const { return !osWindow() && !surface() && !swapchain(); }



} //namespace cth
