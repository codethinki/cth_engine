#pragma once
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <volk.h>

#include <cth/coro/context.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <coroutine>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>


namespace cvk {
class Core;
}

namespace jvk {
/**
 * waits for timeline semaphores on one thread
 * @details
 * - posts the resume of a reached waiter to the context of its caller, the thread resumes no caller
 * - wakes only when a waited value is reached or a smaller wait comes in
 * - no allocation per wait: the waiter lives in the awaiter of the caller, see
 *   `semaphore_awaiter`, the pending list keeps its capacity
 * - thread safe
 * - a created reactor does not move and takes no move assignment: its thread refers to it
 */
class TimelineReactor {
public:
    struct Waiter;

    /**
     * base constructor
     */
    explicit TimelineReactor(cvk::Core const&);

    /**
     * @note calls `optDestroy()`
     */
    ~TimelineReactor() { optDestroy(); }

    /**
     * creates the wake semaphore, starts the thread
     * @pre `cvk::Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    void create();

    /**
     * stops the thread, destroys the wake semaphore
     * @pre `created()`
     * @pre no waiter is pending
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * resumes the caller of the waiter on its context once the semaphore reaches the value
     * @details
     * - posts the resume to `Waiter::contextPtr`, also for a value that is reached already
     * - `Waiter::error` empty -> reached
     * - a failed `vkWaitSemaphores()` of the thread -> every pending waiter gets
     *   `cvk::make_error_code()` of the result in `Waiter::error`, the thread ends, later waiters
     *   get the same code at once
     * - the reactor holds the waiter until the post of its resume, then never touches it again
     * @param[out] waiter with `Waiter::value`, `Waiter::caller` and `Waiter::contextPtr`, gets the
     *  error
     * @pre `created()`
     * @pre the semaphore is a timeline semaphore of the core
     * @pre the semaphore and the waiter are not destroyed while the waiter is pending
     * @throws cvk::vk_result_exception result of `vkSignalSemaphore()` for the wake semaphore, the
     *  waiter stays out
     */
    void await(VkSemaphore, Waiter& waiter) const;

    /**
     * @return a waiter waits for the semaphore
     */
    [[nodiscard]] bool pending(VkSemaphore) const;

    [[nodiscard]] constexpr bool created() const { return _wake.created(); }

private:
    struct Pending {
        VkSemaphore vkSemaphore;
        cth::ref_wrapper<Waiter> waiter;
    };

    void run(std::stop_token);

    /** the caller holds `_mutexPtr` */
    void wake() const;

    /** the caller holds `_mutexPtr` */
    void fail(std::error_code, std::vector<cth::ref_wrapper<Waiter>>& waiters) const;

    /** the caller holds no lock of the reactor */
    static void post(std::span<cth::ref_wrapper<Waiter> const> waiters);

    mutable cvk::TimelineSemaphore _wake;

    // behind a std::unique_ptr: the mutex is not movable, the moves stay = default
    std::unique_ptr<std::mutex> _mutexPtr = std::make_unique<std::mutex>();

    mutable std::vector<Pending> _pending{};

    mutable std::error_code _error{};

    // destroyed first: destroy() stops it before the members
    std::jthread _thread{};

public:
    TimelineReactor(TimelineReactor const& other) = delete;
    TimelineReactor(TimelineReactor&& other) noexcept = default;
    TimelineReactor& operator=(TimelineReactor const& other) = delete;
    TimelineReactor& operator=(TimelineReactor&& other) noexcept = default;
};
}

namespace jvk {
/**
 * suspended wait for a value of a timeline semaphore
 * @details
 * - lives in the awaiter in the frame of the suspended caller, see `semaphore_awaiter`
 * - the reactor reads and writes it until it posts the resume, then never again
 */
struct TimelineReactor::Waiter {
    uint64_t value; ///< to wait for
    std::coroutine_handle<> caller{};
    cth::co::context const* contextPtr = nullptr; ///< of the caller, gets the post of the resume
    std::error_code error{}; ///< the result of a failed wait of the reactor, empty -> reached
};
}
