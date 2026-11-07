#pragma once
#include "cvk/render/sync/timeline_semaphore.hpp"

#include <volk.h>

#include <cth/coro/context.hpp>
#include <cth/data/hash_map.hpp>
#include <cth/ptr/not_null.hpp>

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
//TODO review pending callbacks at the destruction: destroyed without a call, or called with
// std::errc::operation_canceled like asio (see the TODO evaluate of Semaphore::destroy())
/**
 * waits for timeline semaphores on one thread
 * @details posts the callbacks to the worker scheduler, the thread runs no callback
 * @details wakes only when a waited value is reached or a smaller wait comes in
 * @details thread safe
 * @attention the destruction destroys the pending callbacks without a call
 */
class TimelineReactor {
public:
    /**
     * creates the wake semaphore, starts the thread
     * @param workers run the callbacks
     * @pre @ref cvk::Core::created()
     * @throws cvk::vk_result_exception result of vkCreateSemaphore()
     */
    TimelineReactor(cvk::Core const&, cth::co::scheduler const& workers);

    //TODO review const like cth::co::context::await(): Core::reactor() is const, the members are mutable
    //TODO review a failed vkWaitSemaphores() of the thread: error code per callback, std::error_code has no
    // VkResult category
    /**
     * calls the callback on a worker once the semaphore reaches the value
     * @details the callback gets the result of the wait, an empty error_code -> reached
     * @param cb to call
     * @pre the semaphore is a timeline semaphore of the core
     * @pre the semaphore is not destroyed while the callback is pending
     */
    void await(VkSemaphore, uint64_t value, cth::co::context::await_func cb) const;

    //TODO review added for the check of Semaphore::destroy()
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

    /**
     * thread function: waits for the semaphores with waiters and the wake semaphore, posts the reached callbacks
     * @details a stop request raises the wake semaphore, the thread ends
     */
    void run(std::stop_token);

    /**
     * raises the wake semaphore, the thread waits again with the new values
     * @pre the caller holds @ref _mutex
     */
    void wake() const;

    cth::not_null_ptr<cth::co::scheduler const> _workersPtr;

    /**
     * the thread waits for it next to the semaphores with waiters
     */
    mutable cvk::TimelineSemaphore _wake;

    mutable std::mutex _mutex{};

    //TODO review std::vector with the std::ranges heap functions:
    // std::priority_queue cannot move the callback out of top()
    /**
     * min-heap of the waiters by value per semaphore
     * @details an entry exists while the semaphore has waiters
     */
    mutable cth::dt::flat_hash_map<VkSemaphore, std::vector<Waiter>> _waiters{};

    // destroyed first: the stop request raises the wake semaphore, the thread ends before the members
    std::jthread _thread;

public:
    TimelineReactor(TimelineReactor const& other) = delete;
    TimelineReactor(TimelineReactor&& other) = delete;
    TimelineReactor& operator=(TimelineReactor const& other) = delete;
    TimelineReactor& operator=(TimelineReactor&& other) = delete;
};
}
