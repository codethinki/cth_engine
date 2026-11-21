#pragma once
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <volk.h>

#include <cth/coro/context.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>


namespace cvk {
class Core;
}

namespace cth::co {
class scheduler;
}

namespace jvk {
/**
 * waits for timeline semaphores on one thread
 * @details
 * - posts the callbacks to the worker scheduler, the thread runs no callback
 * - wakes only when a waited value is reached or a smaller wait comes in
 * - thread safe
 * - a created reactor does not move and takes no move assignment: its thread refers to it
 */
class TimelineReactor {
public:
    /**
     * base constructor
     * @param workers run the callbacks
     */
    TimelineReactor(cvk::Core const&, cth::co::scheduler const& workers);

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
     * @pre no callback is pending
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
     * calls the callback on a worker once the semaphore reaches the value
     * @details the callback gets the result of the wait:
     * - empty `std::error_code` -> reached
     * - a failed `vkWaitSemaphores()` of the thread -> every pending callback gets `cvk::make_error_code()` of the
     *   result, the thread ends, later callbacks get the same code at once
     * @param cb to call
     * @pre `created()`
     * @pre the semaphore is a timeline semaphore of the core
     * @pre the semaphore is not destroyed while the callback is pending
     * @throws cvk::vk_result_exception result of `vkSignalSemaphore()` for the wake semaphore
     */
    void await(VkSemaphore, uint64_t value, cth::co::context::await_func cb) const;

    /**
     * @return a callback waits for the semaphore
     */
    [[nodiscard]] bool pending(VkSemaphore) const;

    [[nodiscard]] constexpr bool created() const { return _wake.created(); }

private:
    /**
     * callback for a value of a semaphore
     */
    struct Waiter {
        uint64_t value;
        cth::co::context::await_func cb;
    };

    using waiter_map_t = cth::dt::flat_hash_map<VkSemaphore, std::vector<Waiter>>;

    /**
     * thread function: waits for the semaphores with waiters and the wake semaphore, posts the reached callbacks
     * @details stop request -> ends at the next wake
     */
    void run(std::stop_token);

    /**
     * raises the wake semaphore, the thread waits again with the new values
     * @pre the caller holds `TimelineReactor::_mutexPtr`
     * @throws cvk::vk_result_exception result of `vkSignalSemaphore()`
     */
    void wake() const;

    /**
     * stores the error, takes every pending callback
     * @return the callbacks to post with the error
     * @pre the caller holds `TimelineReactor::_mutexPtr`
     */
    [[nodiscard]] std::vector<cth::co::context::await_func> fail(std::error_code) const;

    /**
     * posts the callback to the workers
     */
    void post(cth::co::context::await_func cb, std::error_code) const;

    cth::ref_wrapper<cth::co::scheduler const> _workers;

    /** the thread waits for it next to the semaphores with waiters */
    mutable cvk::TimelineSemaphore _wake;

    // behind a std::unique_ptr: the mutex is not movable, the moves stay = default
    std::unique_ptr<std::mutex> _mutexPtr = std::make_unique<std::mutex>();

    /**
     * min-heap of the waiters by value per semaphore
     * @details an entry exists while the semaphore has waiters
     * @see `std::ranges::push_heap()`
     */
    mutable waiter_map_t _waiters{};

    mutable std::error_code _error{}; ///< the result of a failed wait of the thread, empty -> none

    // destroyed first: destroy() stops it before the members
    std::jthread _thread{};

public:
    TimelineReactor(TimelineReactor const& other) = delete;
    TimelineReactor(TimelineReactor&& other) noexcept = default;
    TimelineReactor& operator=(TimelineReactor const& other) = delete;
    TimelineReactor& operator=(TimelineReactor&& other) noexcept = default;
};
}
