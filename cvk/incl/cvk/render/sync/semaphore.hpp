#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <tuple>


namespace cvk {
class Core;

/**
 * binary `VkSemaphore` wrapper
 */
class Semaphore {
public:
    struct State;

    /**
     * base constructor
     * @pre `Core::created()`
     */
    explicit Semaphore(Core const&);

    /**
     * constructs and wraps
     * @details calls `Semaphore(Core const&)`, `wrap()`
     */
    Semaphore(Core const&, State const&);

    /**
     * constructs and creates
     * @details calls `Semaphore(Core const&)`, `create()`
     */
    explicit Semaphore(Core const&, create_t);

    /**
     * @note calls `optDestroy()`
     */
    ~Semaphore() { optDestroy(); }

    /**
     * wraps the state
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the semaphore
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    void create();

    /**
     * destroys and forgets
     * @pre `created()`
     * @pre the gpu does not use the semaphore
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkSemaphore get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

protected:
    /**
     * creates the semaphore with info
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    void create(VkSemaphoreCreateInfo const& info);

private:
    void forget();

    core_handle<VkSemaphore> _handle;

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
    vk_not_null<VkSemaphore> vkSemaphore;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkSemaphore> decompose() && { return {vkSemaphore.get()}; }
};
}

namespace cvk {
constexpr void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}
}
