#pragma once
#include <vector>
#include "vulkan/utility/cth_constants.hpp"


namespace cth::vk {
class DeletionQueue;
}


namespace cth::vk {
class BasicCore;
class BasicSemaphore;


struct BasicGraphicsSyncConfig {
    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the image once the semaphore is signaled
     */
    std::vector<BasicSemaphore*> renderFinishedSemaphores;

    /**
     * semaphores[currentFrame] will be signaled once the image is free to render on
     * expects that the semaphore will be waited before rendering
     */
    std::vector<BasicSemaphore*> imageAvailableSemaphores;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check_not_null(BasicGraphicsSyncConfig const* config);
    static void debug_check(BasicGraphicsSyncConfig const* config);
#define DEBUG_CHECK_SYNC_CONFIG_NOT_NULL(config) BasicGraphicsSyncConfig::debug_check_not_null(config)
#define DEBUG_CHECK_SYNC_CONFIG(config) BasicGraphicsSyncConfig::debug_check(config)
#else
#define DEBUG_CHECK_SYNC_CONFIG_NOT_NULL(config) ((void)0)
#define DEBUG_CHECK_SYNC_CONFIG(config) ((void)0)
#endif
};

class GraphicsSyncConfig : public BasicGraphicsSyncConfig {
public:
    GraphicsSyncConfig() = default;
    explicit GraphicsSyncConfig(BasicCore const* core, DeletionQueue* deletion_queue);
    ~GraphicsSyncConfig();

    void wrap(BasicGraphicsSyncConfig const& config);
    void create(BasicCore const* core, DeletionQueue* deletion_queue);
    void destroy(DeletionQueue* deletion_queue = nullptr);

    BasicGraphicsSyncConfig release();

    [[nodiscard]] bool destroyed() const;

private:
    void destroyOpt(DeletionQueue* deletion_queue = nullptr);

    BasicCore const* _core = nullptr;
    DeletionQueue* _deletionQueue = nullptr;

public:
    GraphicsSyncConfig(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig(GraphicsSyncConfig&& other) noexcept = default;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig&& other) noexcept = default;
};

} //namespace cth
