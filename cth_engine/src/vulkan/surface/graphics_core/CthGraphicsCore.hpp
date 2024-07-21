#pragma once
#include "CthBasicGraphicsCore.hpp"

namespace cth::vk {
class GraphicsCore : public BasicGraphicsCore {
public:
    explicit GraphicsCore(BasicCore const* core, DeletionQueue* deletion_queue);
    explicit GraphicsCore(BasicCore const* core, DeletionQueue* deletion_queue, std::string_view window_name, VkExtent2D extent,
        Queue const* present_queue, BasicGraphicsSyncConfig const& sync_config);

    ~GraphicsCore() override;

    void wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) override;
    void create(std::string_view window_name, VkExtent2D extent, Queue const* present_queue, BasicGraphicsSyncConfig const& sync_config,
        DeletionQueue* deletion_queue = nullptr) override;

    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue = nullptr;

    [[nodiscard]] bool destroyed() const;
    void optDestroy(DeletionQueue* deletion_queue = nullptr) { if(!destroyed()) destroy(deletion_queue); }

public:
    GraphicsCore(GraphicsCore const& other) = delete;
    GraphicsCore(GraphicsCore&& other) noexcept = default;
    GraphicsCore& operator=(GraphicsCore const& other) = delete;
    GraphicsCore& operator=(GraphicsCore&& other) noexcept = default;
};
} //namespace cth
