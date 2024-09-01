#pragma once
#include <vector>
#include "vulkan/utility/cth_constants.hpp"


namespace cth::vk {
class DestructionQueue;
}


namespace cth::vk {
class BasicCore;
class BasicSemaphore;


struct BasicGraphicsSyncConfig {
    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the vk_image once the semaphore is signaled
     */
    std::vector<BasicSemaphore*> renderFinishedSemaphores;

    /**
     * semaphores[currentFrame] will be signaled once the vk_image is clear to render on
     * expects that the semaphore will be waited before rendering
     */
    std::vector<BasicSemaphore*> imageAvailableSemaphores;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<BasicGraphicsSyncConfig const*> config);
#define DEBUG_CHECK_SYNC_CONFIG(config) BasicGraphicsSyncConfig::debug_check(config)
#else
#define DEBUG_CHECK_SYNC_CONFIG_NOT_NULL(config) ((void)0)
#define DEBUG_CHECK_SYNC_CONFIG(config) ((void)0)
#endif
};

class GraphicsSyncConfig : public BasicGraphicsSyncConfig {
public:
    GraphicsSyncConfig() = default;
    explicit GraphicsSyncConfig(not_null<BasicCore const*> core, DestructionQueue* destruction_queue);
    ~GraphicsSyncConfig();

    void wrap(BasicGraphicsSyncConfig const& config);
    void create(not_null<BasicCore const*> core, DestructionQueue* destruction_queue);
    void destroy(DestructionQueue* destruction_queue = nullptr);

    BasicGraphicsSyncConfig release();

    [[nodiscard]] bool destroyed() const;

private:
    void destroyOpt(DestructionQueue* destruction_queue = nullptr);

    not_null<BasicCore const*> _core;
    DestructionQueue* _destructionQueue = nullptr;

public:
    GraphicsSyncConfig(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig(GraphicsSyncConfig&& other) noexcept = default;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig&& other) noexcept = default;
};

} //namespace cth
