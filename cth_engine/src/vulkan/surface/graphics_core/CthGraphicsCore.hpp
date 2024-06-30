#pragma once
#include "CthBasicGraphicsCore.hpp"

namespace cth {
class GraphicsCore : public BasicGraphicsCore {
public:
    explicit GraphicsCore(const BasicCore* core, DeletionQueue* deletion_queue);
    explicit GraphicsCore(const BasicCore* core, DeletionQueue* deletion_queue, std::string_view window_name, VkExtent2D extent,
        const Queue* present_queue, const BasicGraphicsSyncConfig& sync_config);

    ~GraphicsCore() override;

    void wrap(OSWindow* os_window, Surface* surface, BasicSwapchain* swapchain) override;
    void create(std::string_view window_name, VkExtent2D extent, const Queue* present_queue, const BasicGraphicsSyncConfig& sync_config,
        DeletionQueue* deletion_queue = nullptr) override;

    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue = nullptr;

    [[nodiscard]] bool destroyed() const;
    void optDestroy(DeletionQueue* deletion_queue = nullptr) { if(!destroyed()) destroy(deletion_queue); }

public:
    GraphicsCore(const GraphicsCore& other) = delete;
    GraphicsCore(GraphicsCore&& other) noexcept = default;
    GraphicsCore& operator=(const GraphicsCore& other) = delete;
    GraphicsCore& operator=(GraphicsCore&& other) noexcept = default;
};
} //namespace cth
