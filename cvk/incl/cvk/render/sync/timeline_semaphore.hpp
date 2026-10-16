#pragma once
#include "semaphore.hpp"

#include <atomic>
#include <cstdint>


namespace cvk {
/**
 * @brief timeline VkSemaphore with a host counter
 * @details the counter is the last value that @ref next() or @ref signal() gave out,
 *  private base: a timeline semaphore must not pass as a binary @ref Semaphore,
 *  move members are custom: the counter is atomic
 */
class TimelineSemaphore : Semaphore {
public:
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    explicit TimelineSemaphore(Core const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref TimelineSemaphore(Core const&)
     * - @ref wrap()
     */
    TimelineSemaphore(Core const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref TimelineSemaphore(Core const&)
     * - @ref create()
     */
    explicit TimelineSemaphore(Core const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~TimelineSemaphore() = default;

    /**
     * @brief wraps the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the semaphore
     * @param initialValue of the semaphore and the counter
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    void create(uint64_t initialValue = 0);

    using Semaphore::destroy;
    using Semaphore::optDestroy;

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief advances the counter and signals the semaphore from the host
     * @return the signaled value
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkSignalSemaphore()
     */
    uint64_t signal();

    /**
     * @brief blocks until the semaphore reaches @ref value() or the timeout ends
     * @param timeout in nanoseconds
     * @return VK_SUCCESS or VK_TIMEOUT
     * @pre @ref created()
     * @throws cvk::vk_result_exception other result of vkWaitSemaphores()
     */
    [[nodiscard]] VkResult wait(uint64_t timeout) const;

    /**
     * @brief blocks until the semaphore reaches @ref value()
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkWaitSemaphores()
     */
    void wait() const;

    /**
     * @brief advances the counter
     * @return value for the next signal operation
     * @details thread safe
     */
    [[nodiscard]] uint64_t next() const { return ++_value; }

    /**
     * @return the counter, value to wait for
     */
    [[nodiscard]] uint64_t value() const { return _value; }

    /**
     * @return current value of the semaphore
     * @pre @ref created()
     * @throws cvk::vk_result_exception result of vkGetSemaphoreCounterValue()
     */
    [[nodiscard]] uint64_t gpuValue() const;

    using Semaphore::core;
    using Semaphore::created;
    using Semaphore::get;

private:
    mutable std::atomic<uint64_t> _value = 0;

public:
    TimelineSemaphore(TimelineSemaphore const& other) = delete;
    TimelineSemaphore(TimelineSemaphore&& other) noexcept : Semaphore{std::move(other)},
        _value{other._value.exchange(0)} {}
    TimelineSemaphore& operator=(TimelineSemaphore const& other) = delete;
    TimelineSemaphore& operator=(TimelineSemaphore&& other) noexcept {
        Semaphore::operator=(std::move(other));
        _value = other._value.exchange(0);
        return *this;
    }

    static void debug_check(TimelineSemaphore const& semaphore);
};
}

namespace cvk {
struct TimelineSemaphore::State {
    vk_not_null<VkSemaphore> vkSemaphore;
    /**
     * @brief counter, see @ref value()
     */
    uint64_t value;
};
}

namespace cvk {
inline void TimelineSemaphore::debug_check(TimelineSemaphore const& semaphore) { Semaphore::debug_check(semaphore); }
}
