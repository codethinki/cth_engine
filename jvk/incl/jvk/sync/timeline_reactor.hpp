#pragma once
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <volk.h>

#include <cth/coro/context.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <cstdint>
#include <mutex>
#include <stop_token>
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
 */
class TimelineReactor {
public:
    /**
     * creates the wake semaphore, starts the thread
     * @param workers run the callbacks
     * @pre `cvk::Core::created()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    TimelineReactor(cvk::Core const&, cth::co::scheduler const& workers);

    /**
     * stops the thread
     * @pre no callback is pending
     */
    ~TimelineReactor() = default;

    //TODO review const like cth::co::context::await(): Core::reactor() is const, the members are mutable
    //TODO review a failed vkWaitSemaphores() of the thread: error code per callback, std::error_code has no
    // VkResult category
    /**
     * calls the callback on a worker once the semaphore reaches the value
     * @details the callback gets the result of the wait: empty `std::error_code` -> reached
     * @param cb to call
     * @pre the semaphore is a timeline semaphore of the core
     * @pre the semaphore is not destroyed while the callback is pending
     */
    void await(VkSemaphore, uint64_t value, cth::co::context::await_func cb) const;

    /**
     * @return a callback waits for the semaphore
     */
    [[nodiscard]] bool pending(VkSemaphore) const;

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
     * @details stop request -> raises the wake semaphore, the thread ends
     */
    void run(std::stop_token);

    /**
     * raises the wake semaphore, the thread waits again with the new values
     * @pre the caller holds `TimelineReactor::_mutex`
     */
    void wake() const;

    cth::ref_wrapper<cth::co::scheduler const> _workers;

    mutable cvk::TimelineSemaphore _wake; ///< the thread waits for it next to the semaphores with waiters

    mutable std::mutex _mutex{};

    /**
     * min-heap of the waiters by value per semaphore
     * @details an entry exists while the semaphore has waiters
     * @see `std::ranges::push_heap()`
     */
    mutable waiter_map_t _waiters{};

    // destroyed first: the stop request raises the wake semaphore, the thread ends before the members
    std::jthread _thread;

public:
    TimelineReactor(TimelineReactor const& other) = delete;
    TimelineReactor(TimelineReactor&& other) = delete;
    TimelineReactor& operator=(TimelineReactor const& other) = delete;
    TimelineReactor& operator=(TimelineReactor&& other) = delete;
};
}
