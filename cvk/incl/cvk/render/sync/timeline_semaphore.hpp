#pragma once
#include "semaphore.hpp"

#include <cstdint>
#include <tuple>


namespace cvk {
/**
 * timeline `VkSemaphore` wrapper
 * @details host counter: the last value that `next()` or `signal()` gave out
 * @attention not thread safe: `next()` and `signal()` change the counter
 */
class TimelineSemaphore : Semaphore {
public:
    struct State;

    /**
     * base constructor
     * @pre `Core::created()`
     */
    explicit TimelineSemaphore(Core const&);

    /**
     * constructs and wraps
     * @details calls `TimelineSemaphore(Core const&)`, `wrap()`
     */
    TimelineSemaphore(Core const&, State const&);

    /**
     * constructs and creates
     * @details calls `TimelineSemaphore(Core const&)`, `create()` with the initial value
     */
    explicit TimelineSemaphore(Core const&, create_t, uint64_t initialValue = 0);

    /**
     * @note calls `optDestroy()`
     */
    ~TimelineSemaphore() = default;

    /**
     * wraps the state
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the semaphore
     * @param initialValue of the semaphore and the counter
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    void create(uint64_t initialValue = 0);

    using Semaphore::destroy;
    using Semaphore::optDestroy;

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    /**
     * advances the counter and signals the semaphore from the host
     * @return the signaled value
     * @pre `created()`
     * @pre no signal of the semaphore is pending
     * @throws cvk::vk_result_exception result of `vkSignalSemaphore()`
     */
    uint64_t signal();

    /**
     * blocks until the semaphore reaches the value or the timeout ends
     * @param timeout in nanoseconds
     * @return `VK_SUCCESS` or `VK_TIMEOUT`
     * @pre `created()`
     * @throws cvk::vk_result_exception other result of `vkWaitSemaphores()`
     */
    [[nodiscard]] VkResult wait(uint64_t value, uint64_t timeout) const;

    /**
     * blocks until the semaphore reaches the value
     * @pre `created()`
     * @throws cvk::vk_result_exception result of `vkWaitSemaphores()`
     */
    void wait(uint64_t value) const;

    /**
     * blocks until the semaphore reaches `value()`
     */
    void wait() const { wait(_value); }

    /**
     * advances the counter
     * @return value for the next signal operation
     * @attention signal the values in counter order: two queues need a wait between their submits
     */
    [[nodiscard]] constexpr uint64_t next() const { return ++_value; }

    /**
     * @return the counter, value to wait for
     */
    [[nodiscard]] constexpr uint64_t value() const { return _value; }

    /**
     * @return current value of the semaphore
     * @pre `created()`
     * @throws cvk::vk_result_exception result of `vkGetSemaphoreCounterValue()`
     */
    [[nodiscard]] uint64_t gpuValue() const;

    using Semaphore::core;
    using Semaphore::created;
    using Semaphore::get;

private:
    mutable uint64_t _value = 0;

public:
    TimelineSemaphore(TimelineSemaphore const& other) = delete;
    TimelineSemaphore(TimelineSemaphore&& other) noexcept = default;
    TimelineSemaphore& operator=(TimelineSemaphore const& other) = delete;
    TimelineSemaphore& operator=(TimelineSemaphore&& other) noexcept = default;

    static constexpr void debug_check(TimelineSemaphore const& semaphore);
};
}

namespace cvk {
struct TimelineSemaphore::State {
    vk_not_null<VkSemaphore> vkSemaphore;
    /**
     * counter, see `value()`
     */
    uint64_t value;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkSemaphore> decompose() && { return {vkSemaphore.get()}; }
};
}

namespace cvk {
constexpr void TimelineSemaphore::debug_check(TimelineSemaphore const& semaphore) { Semaphore::debug_check(semaphore); }
}
