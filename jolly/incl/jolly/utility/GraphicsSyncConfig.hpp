#pragma once
#include "types.hpp"

#include "jolly/render/RenderPulse.hpp"
#include "jvk/render/sync/pipeline_wait_stage.hpp"
#include "jvk/utility/constants.hpp"

#include <cth/io/log.hpp>
#include <cth/pointer/not_null.hpp>

#include <vector>

namespace jvk {
class Semaphore;
class Core;
}

namespace jly {
class GraphicsSyncConfig {
public:
    static constexpr auto SET_SIZE = jvk::constants::FRAMES_IN_FLIGHT;
    struct State;

    /**
     * @brief base constructor
     * @param size number of semaphores to create (!= 0)
     */
    explicit GraphicsSyncConfig(jvk::Core const& core, size_t size = jvk::constants::FRAMES_IN_FLIGHT);

    /**
     * @brief constructs and wraps
     * @details calls @ref GraphicsSyncConfig(jvk::Core const&, size_t)
     * @note calls @ref wrap()
     */
    GraphicsSyncConfig(jvk::Core const& core, State state, size_t size = jvk::constants::FRAMES_IN_FLIGHT);

    /**
     * constructs and creates if create
     * @details calls:
        - @ref GraphicsSyncConfig(jvk::Core const&, size_t) with size
        - @ref create()
     */
    GraphicsSyncConfig(jvk::Core const& core, create_t, size_t size = jvk::constants::FRAMES_IN_FLIGHT);

    /**
     * @details calls @ref optDestroy()
     */
    ~GraphicsSyncConfig();

    /**
     * @brief wraps @ref State
     * @details calls @ref optDestroy()
     */
    void wrap(State state);
    /**
     * @brief creates the semaphores
     * @details calls:
            @ref optDestroy()
            @ref Semaphore::Semaphore(Core const&, create_t)
     */
    void create();

    /**
     * @brief destroys and resets
     * @attention @ref created() required
     * @note calls @ref Semaphore::~Semaphore()
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @attention @ref created() required
     */
    State release();

    /**
     * @brief next pulse
     * @details calls RenderPulse::next()
     */
    void next() { _pulse.next(); }


    [[nodiscard]] std::vector<jvk::Semaphore*> renderFinishedSemaphores();
    [[nodiscard]] std::vector<jvk::Semaphore*> imageAvailableSemaphores();
    [[nodiscard]] std::vector<jvk::Semaphore const*> renderFinishedSemaphores() const;
    [[nodiscard]] std::vector<jvk::Semaphore const*> imageAvailableSemaphores() const;
    [[nodiscard]] std::vector<jvk::PipelineWaitStage> imageAvailableWaitStages() const;

private:
    cth::not_null<jvk::Core const*> _core;
    size_t _size;

    RenderPulse _pulse{};

    /**
     * semaphores[currentFrame] will be signaled once the vk_image is clear to render on
     * expects that the semaphore will be waited before rendering
     */
    std::vector<jvk::Semaphore> _imageAvailableSemaphores;

    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the vk_image once the semaphore is signaled
     */
    std::vector<jvk::Semaphore> _renderFinishedSemaphores;

public:
    [[nodiscard]] bool created() const {
        return !_imageAvailableSemaphores.empty() && !_renderFinishedSemaphores.empty();
    }

    [[nodiscard]] jvk::Semaphore const* renderFinishedSemaphore(size_t index) const;
    [[nodiscard]] jvk::Semaphore* renderFinishedSemaphore(size_t index);
    [[nodiscard]] jvk::Semaphore const* imageAvailableSemaphore(size_t index) const;
    [[nodiscard]] jvk::Semaphore* imageAvailableSemaphore(size_t index);

    [[nodiscard]] RenderPulse const& pulse() const { return _pulse; }
    [[nodiscard]] dclauto pulseVal() const { return _pulse.get(); }

    GraphicsSyncConfig(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig(GraphicsSyncConfig&& other) noexcept = default;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig&& other) noexcept = default;

    static void debug_check(GraphicsSyncConfig const& config);
};
}


//State

namespace jly {
struct GraphicsSyncConfig::State {
    RenderPulse pulse;
    /**
     * @attention must not be nullptr
     */
    std::vector<jvk::Semaphore> imageAvailableSemaphores;
    /**
     * @attention must not be nullptr
     */
    std::vector<jvk::Semaphore> renderFinishedSemaphores;

private:
    static void debug_check(State const& state);

    friend GraphicsSyncConfig;
};
}


//debug checks

namespace jly {
inline void GraphicsSyncConfig::debug_check(GraphicsSyncConfig const& config) {
    CTH_CRITICAL(!config.created(), "config not created") {}
}
}
