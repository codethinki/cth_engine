#pragma once
#include "vulkan/render/control/CthSemaphore.hpp"
#include "vulkan/utility/cth_constants.hpp"

#include <vector>


namespace cth::vk {
class BasicCore;


class GraphicsSyncConfig {
public:
    static constexpr auto SET_SIZE = constants::FRAMES_IN_FLIGHT;
    struct State;

    /**
     * @brief base constructor
     */
    explicit GraphicsSyncConfig(not_null<BasicCore const*> core) : _core{core} {}

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     */
    GraphicsSyncConfig(not_null<BasicCore const*> core, State state);

    /**
     * @brief constructs and creates if create
    * @note may call @ref create()
     */
    GraphicsSyncConfig(not_null<BasicCore const*> core, bool create);

    /**
     * @note calls @ref optDestroy()
     */
    ~GraphicsSyncConfig() { optDestroy(); }

    /**
     * @brief wraps semaphores in @param state
     * @note calls @ref optDestroy()
     */
    void wrap(State state);
    /**
     * @brief creates the semaphores
     * @note calls @ref optDestroy()
     * @note calls @ref Semaphore::Semaphore(BasicCore*, bool) i.e. create constructor
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


    [[nodiscard]] std::array<BasicSemaphore*, SET_SIZE> renderFinishedSemaphores() const;
    [[nodiscard]] std::array<BasicSemaphore*, SET_SIZE> imageAvailableSemaphores() const;

private:
    not_null<BasicCore const*> _core;

    /**
     * semaphores[currentFrame] will be signaled once the vk_image is clear to render on
     * expects that the semaphore will be waited before rendering
     */
    std::array<std::unique_ptr<BasicSemaphore>, SET_SIZE> _imageAvailableSemaphores;

    /**
     * expects semaphores[currentFrame] to be signaled after rendering
     * presents the vk_image once the semaphore is signaled
     */
    std::array<std::unique_ptr<BasicSemaphore>, SET_SIZE> _renderFinishedSemaphores;

public:
    [[nodiscard]] bool created() const { return _imageAvailableSemaphores[0] && _renderFinishedSemaphores[0]; }
    [[nodiscard]] BasicSemaphore* renderFinishedSemaphore(size_t index) const { return _renderFinishedSemaphores[index].get(); }
    [[nodiscard]] BasicSemaphore* imageAvailableSemaphore(size_t index) const { return _imageAvailableSemaphores[index].get(); }

    GraphicsSyncConfig(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig const& other) = delete;
    GraphicsSyncConfig(GraphicsSyncConfig&& other) noexcept = default;
    GraphicsSyncConfig& operator=(GraphicsSyncConfig&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(not_null<GraphicsSyncConfig const*> config);
    static void debug_check_state(State const& state);
#define DEBUG_CHECK_GRAPHICS_SYNC_CONFIG(config) GraphicsSyncConfig::debug_check(config)

#define DEBUG_CHECK_GRAPHICS_SYNC_CONFIG_STATE(state) 
#else
#define DEBUG_CHECK_SYNC_CONFIG_NOT_NULL(config) ((void)0)
#define DEBUG_CHECK_GRAPHICS_SYNC_CONFIG(config) ((void)0)
#endif
};
}

//State

namespace cth::vk {
struct GraphicsSyncConfig::State {
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<BasicSemaphore>, constants::FRAMES_IN_FLIGHT> imageAvailableSemaphores;
    /**
     * @attention must not be nullptr
     */
    std::array<std::unique_ptr<BasicSemaphore>, constants::FRAMES_IN_FLIGHT> renderFinishedSemaphores;
};
}
