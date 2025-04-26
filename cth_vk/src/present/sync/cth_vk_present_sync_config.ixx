module;
#include <cth/io/io_log.hpp>


export module cth.vk.present.sync.config;

import cth.vk.render.sync.pulse;
import cth.vk.render.sync.pipeline_wait_stage;
import cth.vk.constants;
import cth.vk.render.sync.semaphore;
import cth.vk.base.core;

import cth.io.log;
import cth.ptr.not_null;

import std;

export namespace cth::vk {
class PresentSyncConfig {
public:
    static constexpr auto SET_SIZE = constants::FRAMES_IN_FLIGHT;
    struct State;

    /**
     * @brief base constructor
     */
    explicit PresentSyncConfig(Core const& core);

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     */
    PresentSyncConfig(Core const& core, State state);

    /**
     * @brief constructs and creates if create
    * @note may call @ref create()
     */
    PresentSyncConfig(Core const& core, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~PresentSyncConfig();

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


    [[nodiscard]] std::array<Semaphore*, SET_SIZE> renderFinishedSemaphores() const;
    [[nodiscard]] std::array<Semaphore*, SET_SIZE> imageAvailableSemaphores() const;
    [[nodiscard]] std::vector<PipelineWaitStage> imageAvailableWaitStages() const;

private:
    cth::not_null<Core const*> _core;

    RenderPulse _pulse{};

    /**
     * semaphores[currentFrame] will be signaled once the vk_image is clear to render on
     * expects that the semaphore will be waited before rendering
     */
    std::array<std::unique_ptr<Semaphore>, SET_SIZE> _imageAvailableSemaphores;

    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the vk_image once the semaphore is signaled
     */
    std::array<std::unique_ptr<Semaphore>, SET_SIZE> _renderFinishedSemaphores;

public:
    [[nodiscard]] bool created() const {
        return std::ranges::none_of(_imageAvailableSemaphores, [](auto const& ptr) { return ptr == nullptr; }) //TODO make this faster
            && std::ranges::none_of(_imageAvailableSemaphores, [](auto const& ptr) { return ptr == nullptr; });
    }
    [[nodiscard]] Semaphore* renderFinishedSemaphore(size_t index) const;
    [[nodiscard]] Semaphore* imageAvailableSemaphore(size_t index) const;

    [[nodiscard]] RenderPulse const& pulse() const { return _pulse; }
    [[nodiscard]] dclauto pulseVal() const { return _pulse.get(); }

    PresentSyncConfig(PresentSyncConfig const& other) = delete;
    PresentSyncConfig& operator=(PresentSyncConfig const& other) = delete;
    PresentSyncConfig(PresentSyncConfig&& other) noexcept = default;
    PresentSyncConfig& operator=(PresentSyncConfig&& other) noexcept = default;

    static void debug_check(PresentSyncConfig const& config);
};
}


//State


export namespace cth::vk {
struct PresentSyncConfig::State {
    RenderPulse pulse;
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<Semaphore>, constants::FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<Semaphore>, constants::FRAMES_IN_FLIGHT> renderFinishedSemaphores;

private:
    static void debug_check(State const& state);

    friend PresentSyncConfig;
};
}


//debug checks

export namespace cth::vk {
inline void PresentSyncConfig::debug_check(PresentSyncConfig const& config) {
    CTH_CRITICAL(!config.created(), "config not created"){}
}
}
