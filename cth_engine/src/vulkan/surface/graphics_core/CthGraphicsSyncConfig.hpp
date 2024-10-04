#pragma once
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include <vector>


namespace cth::vk {
class Core;


class GraphicsSyncConfig {
public:
    static constexpr auto SET_SIZE = constants::FRAMES_IN_FLIGHT;
    struct State;

    /**
     * @brief base constructor
     */
    explicit GraphicsSyncConfig(cth::not_null<Core const*> core) : _core{core} {}

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     */
    GraphicsSyncConfig(cth::not_null<Core const*> core, State state);

    /**
     * @brief constructs and creates if create
    * @note may call @ref create()
     */
    GraphicsSyncConfig(cth::not_null<Core const*> core, bool create);

    /**
     * @note calls @ref optDestroy()
     */
    ~GraphicsSyncConfig() { optDestroy(); }

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State state);
    /**
     * @brief creates the semaphores
     * @note calls @ref optDestroy()
     * @note calls @ref Semaphore::Semaphore(Core*, bool) i.e. create constructor
     */
    void create();

    /**
     * @brief destroys and resets
     * @note calls @ref Semaphore::~Semaphore()
     * @note requires @ref created()
     */
    void destroy();
    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @note requires @ref created()
     */
    State release();


    [[nodiscard]] std::array<Semaphore*, SET_SIZE> renderFinishedSemaphores() const;
    [[nodiscard]] std::array<Semaphore*, SET_SIZE> imageAvailableSemaphores() const;

private:
    cth::not_null<Core const*> _core;

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
    [[nodiscard]] bool created() const { return _imageAvailableSemaphores[0] && _renderFinishedSemaphores[0]; }
    [[nodiscard]] Semaphore* renderFinishedSemaphore(size_t index) const { return _renderFinishedSemaphores[index].get(); }
    [[nodiscard]] Semaphore* imageAvailableSemaphore(size_t index) const { return _imageAvailableSemaphores[index].get(); }

    GraphicsSyncConfig(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig(GraphicsSyncConfig&& other) noexcept = default;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig&& other) noexcept = default;

    static void debug_check(cth::not_null<GraphicsSyncConfig const*> config);
    static void debug_check_state(State const& state);
};
}

//State

namespace cth::vk {
struct GraphicsSyncConfig::State {
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<Semaphore>, constants::FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<Semaphore>, constants::FRAMES_IN_FLIGHT> renderFinishedSemaphores;
};
}

//debug checks

namespace cth::vk {
inline void GraphicsSyncConfig::debug_check(cth::not_null<GraphicsSyncConfig const*> config) {
    CTH_CRITICAL(!config->created(), "config not created"){}
}
inline void GraphicsSyncConfig::debug_check_state(State const& state) {
    for(auto& semaphore : state.imageAvailableSemaphores)
        Semaphore::debug_check(semaphore.get());
    for(auto& semaphore : state.renderFinishedSemaphores)
        Semaphore::debug_check(semaphore.get());
}
}
