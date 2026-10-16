#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>

#include <cth/io/log.hpp>


namespace cvk {
class Core;

/**
 * @brief binary VkSemaphore wrapper
 */
class Semaphore {
public:
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    explicit Semaphore(Core const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Semaphore(Core const&)
     * - @ref wrap()
     */
    Semaphore(Core const&, State const&);

    /**
     * @brief constructs and creates
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
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the semaphore
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    void create();

    /**
     * @brief destroys and invalidates
     * @pre @ref created()
     * @pre the gpu does not use the semaphore
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and invalidates
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkSemaphore);

    [[nodiscard]] bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }
    [[nodiscard]] Core const& core() const { return _handle.core(); }

protected:
    /**
     * @brief creates the semaphore with info
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    void create(VkSemaphoreCreateInfo const& info);

private:
    void invalidate();

    unique_handle<VkSemaphore, core_deleter<Semaphore>> _handle;

public:
    Semaphore(Semaphore const& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore const& other) = delete;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

    static void debug_check(Semaphore const&);
};
}

namespace cvk {
struct Semaphore::State {
    vk_not_null<VkSemaphore> vkSemaphore;
};
}

namespace cvk {
inline void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}
}
