#pragma once
#include "src/interface/render/renderer3/RenderPulse.hpp"
#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"
#include "src/vulkan/utility/cth_constants.hpp"

#include <cth/io/log.hpp>
#include <cth/pointer/not_null.hpp>

#include <vector>

namespace cth::vk {
class Semaphore;
class Core;


class GraphicsSyncConfig {
public:
    static constexpr auto SET_SIZE = constants::FRAMES_IN_FLIGHT;
    struct State;

    /**
     * @brief base constructor
     */
    explicit GraphicsSyncConfig(Core const& core);

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     */
    GraphicsSyncConfig(Core const& core, State state);

    /**
     * @brief constructs and creates if create
    * @note may call @ref create()
     */
    GraphicsSyncConfig(Core const& core, create_t);

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

    /**
     * @brief signals the render-finished semaphores
     * @details calls @ref Semaphore::signal() 
     */
    void signal();


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

    GraphicsSyncConfig(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig(GraphicsSyncConfig&& other) noexcept = default;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig&& other) noexcept = default;

    static void debug_check(GraphicsSyncConfig const& config);
};
}


//State

namespace cth::vk {
struct GraphicsSyncConfig::State {
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

    friend GraphicsSyncConfig;
};
}


//debug checks

namespace cth::vk {
inline void GraphicsSyncConfig::debug_check(GraphicsSyncConfig const& config) {
    CTH_CRITICAL(!config.created(), "config not created"){}
}
}
