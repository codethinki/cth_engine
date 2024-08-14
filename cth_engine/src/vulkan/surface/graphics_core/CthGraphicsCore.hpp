#pragma once
#include "CthBasicGraphicsCore.hpp"

namespace cth::vk {
class GraphicsCore : public BasicGraphicsCore {
public:
    explicit GraphicsCore(BasicCore const* core);
    explicit GraphicsCore(BasicCore const* core, std::string_view window_name, VkExtent2D extent,
        Queue const* present_queue, BasicGraphicsSyncConfig const& sync_config);

    ~GraphicsCore() override;

    void wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) override;
    void create(std::string_view const window_name, VkExtent2D const extent, Queue const* present_queue, BasicGraphicsSyncConfig const* sync_config,
        DestructionQueue* destruction_queue = nullptr) override;

    void destroy(DestructionQueue* destruction_queue = nullptr) override;

private:


    [[nodiscard]] bool destroyed() const;
    void optDestroy(DestructionQueue* destruction_queue = nullptr) { if(!destroyed()) destroy(destruction_queue); }

public:
    GraphicsCore(GraphicsCore const& other) = delete;
    GraphicsCore(GraphicsCore&& other) noexcept = default;
    GraphicsCore& operator=(GraphicsCore const& other) = delete;
    GraphicsCore& operator=(GraphicsCore&& other) noexcept = default;
};
} //namespace cth
