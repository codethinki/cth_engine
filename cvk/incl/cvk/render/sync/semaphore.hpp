#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/deletable.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <utility>


namespace cvk {
class Core;

/**
 * binary VkSemaphore wrapper
 */
class Semaphore {
public:
    using vk_handle_t = VkSemaphore;
    struct State;

    /**
     * base constructor
     * @pre @ref Core::created()
     */
    explicit Semaphore(Core const&);

    /**
     * constructs and wraps
     * @details calls:
     * - @ref Semaphore(Core const&)
     * - @ref wrap()
     */
    Semaphore(Core const&, State const&);

    /**
     * constructs and creates
     * @details calls:
     * - @ref Semaphore(Core const&)
     * - @ref create()
     */
    explicit Semaphore(Core const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Semaphore() { optDestroy(); }

    /**
     * wraps the state
     * @pre the semaphore of the state belongs to the device of the core
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * creates the semaphore
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    void create();

    /**
     * destroys and forgets
     * @pre @ref created()
     * @pre the gpu does not use the semaphore
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @return the state, its deletable has the device table of the core
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkSemaphore);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

protected:
    /**
     * creates the semaphore with info
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    void create(VkSemaphoreCreateInfo const& info);

private:
    void forget();

    core_handle<Semaphore> _handle;

public:
    Semaphore(Semaphore const& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore const& other) = delete;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

    static constexpr void debug_check(Semaphore const&);
};
}

namespace cvk {
struct Semaphore::State {
    device_deletable<Semaphore> semaphore;

    /**
     * consumes the state into its deletable
     */
    [[nodiscard]] constexpr device_deletable<Semaphore> collapse() && { return std::move(semaphore); }
};
}

namespace cvk {
constexpr void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}
}
