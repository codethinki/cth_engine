#pragma once
#include "types.hpp"

#include "jolly/render/RenderPulse.hpp"
#include "jvk/render/ctrl/pipeline_wait_stage.hpp"
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
     */
    explicit GraphicsSyncConfig(jvk::Core const& core);

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     */
    GraphicsSyncConfig(jvk::Core const& core, State state);

    /**
     * @brief constructs and creates if create
    * @note may call @ref create()
     */
    GraphicsSyncConfig(jvk::Core const& core, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~GraphicsSyncConfig();

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
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


    [[nodiscard]] std::array<jvk::Semaphore*, SET_SIZE> renderFinishedSemaphores() const;
    [[nodiscard]] std::array<jvk::Semaphore*, SET_SIZE> imageAvailableSemaphores() const;
    [[nodiscard]] std::vector<jvk::PipelineWaitStage> imageAvailableWaitStages() const;

private:
    cth::not_null<jvk::Core const*> _core;

    RenderPulse _pulse{};

    /**
     * semaphores[currentFrame] will be signaled once the vk_image is clear to render on
     * expects that the semaphore will be waited before rendering
     */
    std::array<std::unique_ptr<jvk::Semaphore>, SET_SIZE> _imageAvailableSemaphores;

    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the vk_image once the semaphore is signaled
     */
    std::array<std::unique_ptr<jvk::Semaphore>, SET_SIZE> _renderFinishedSemaphores;

public:
    [[nodiscard]] bool created() const {
        return std::ranges::none_of(_imageAvailableSemaphores, [](auto const& ptr) { return ptr == nullptr; })
            //TODO make this faster
            && std::ranges::none_of(_imageAvailableSemaphores, [](auto const& ptr) {
                return ptr == nullptr;
            });
    }

    [[nodiscard]] jvk::Semaphore* renderFinishedSemaphore(size_t index) const;
    [[nodiscard]] jvk::Semaphore* imageAvailableSemaphore(size_t index) const;

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
    std::array<std::unique_ptr<jvk::Semaphore>, constants::FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<Semaphore>, constants::FRAMES_IN_FLIGHT> renderFinishedSemaphores;

private:
    static void debug_check(State const& state);

    friend GraphicsSyncConfig;
};
}


//debug checks

namespace jvk {
inline void GraphicsSyncConfig::debug_check(GraphicsSyncConfig const& config) {
    CTH_CRITICAL(!config.created(), "config not created") {}
}
}
