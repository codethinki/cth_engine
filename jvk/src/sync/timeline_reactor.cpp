#include "jvk/sync/timeline_reactor.hpp"

#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_error_code.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/io/log.hpp>

#include <algorithm>
#include <exception>
#include <limits>
#include <span>
#include <utility>
#include <vector>


namespace jvk {
using std::vector;

TimelineReactor::TimelineReactor(cvk::Core const& core) : _wake{core} {}

void TimelineReactor::create() {
    optDestroy();

    _wake.create();
    _error = {};

    try {
        _thread = std::jthread{[this](std::stop_token stop) { run(std::move(stop)); }};
    }
    catch(...) {
        _wake.destroy();
        throw;
    }
}

void TimelineReactor::destroy() {
    CTH_CRITICAL(!created(), "reactor must be created") {}

    {
        std::scoped_lock const lock{*_mutexPtr};
        CTH_CRITICAL(
            !_pending.empty(),
            "no waiter must be pending, got {}",
            _pending.size()
        ) {}

        _thread.request_stop();

        // a failed wait ended the thread already, the wake does not matter then
        try { wake(); }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(
                !_error,
                "failed to wake the reactor in destroy(), got {}",
                e.vkResult()
            ) {}
        }
    }

    _thread.join();
    _thread = {};

    _pending.clear();
    _error = {};
    _wake.destroy();
}

void TimelineReactor::await(VkSemaphore vkSemaphore, Waiter& waiter) const {
    CTH_CRITICAL(!created(), "reactor must be created") {}
    CTH_CRITICAL(vkSemaphore == VK_NULL_HANDLE, "the semaphore must not be VK_NULL_HANDLE") {}
    CTH_CRITICAL(waiter.contextPtr == nullptr, "the waiter must have the context of its caller") {}

    {
        std::scoped_lock const lock{*_mutexPtr};

        if(!_error) {
            // the thread waits for the smallest value per semaphore, a smaller one needs a new wait
            bool const smallest = std::ranges::none_of(
                _pending,
                [&](Pending const& pending) {
                    return pending.vkSemaphore == vkSemaphore
                        && pending.waiter->value <= waiter.value;
                }
            );
            if(smallest)
                wake();

            // after the wake: a failed wake registers nothing
            _pending.push_back({.vkSemaphore = vkSemaphore, .waiter = waiter});
            return;
        }

        waiter.error = _error;
    }

    cth::ref_wrapper<Waiter> const failed{waiter};
    post({&failed, 1});
}

bool TimelineReactor::pending(VkSemaphore vkSemaphore) const {
    std::scoped_lock const lock{*_mutexPtr};
    return std::ranges::contains(_pending, vkSemaphore, &Pending::vkSemaphore);
}

void TimelineReactor::run(std::stop_token stop) {
    auto const table = _wake.core().deviceTable();

    // reused per round: the vectors keep their capacity
    // the wait arrays: the wake semaphore first, then each semaphore with its smallest value
    vector<VkSemaphore> vkSemaphores{};
    vector<uint64_t> values{};
    // the counter per semaphore of the wait arrays
    vector<uint64_t> counters{};
    // the waiters to post, after a failure every pending one
    vector<cth::ref_wrapper<Waiter>> reached{};
    std::error_code error{};

    // the index of the semaphore in the wait arrays, the size -> not in them
    auto const indexOf = [&vkSemaphores](VkSemaphore vkSemaphore) {
        auto const it = std::ranges::find(vkSemaphores, vkSemaphore);
        return static_cast<size_t>(it - vkSemaphores.begin());
    };

    while(true) {
        {
            std::scoped_lock const lock{*_mutexPtr};
            if(stop.stop_requested())
                return;

            vkSemaphores.assign({_wake.get()});
            values.assign({_wake.value() + 1});
            for(auto const& [vkSemaphore, waiter] : _pending) {
                auto const semaphoreIdx = indexOf(vkSemaphore);
                if(semaphoreIdx < vkSemaphores.size())
                    values[semaphoreIdx] = std::min(values[semaphoreIdx], waiter->value);
                else {
                    vkSemaphores.push_back(vkSemaphore);
                    values.push_back(waiter->value);
                }
            }
        }

        VkSemaphoreWaitInfo const info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext = nullptr,
            .flags = VK_SEMAPHORE_WAIT_ANY_BIT,
            .semaphoreCount = static_cast<uint32_t>(vkSemaphores.size()),
            .pSemaphores = vkSemaphores.data(),
            .pValues = values.data(),
        };
        auto result = table->vkWaitSemaphores(
            table.device(),
            &info,
            std::numeric_limits<uint64_t>::max()
        );

        // off the lock: the semaphores of the wait arrays keep their waiters until the partition
        // below, the wake semaphore needs no counter
        counters.resize(vkSemaphores.size());
        for(size_t semaphoreIdx = 1; semaphoreIdx < vkSemaphores.size(); ++semaphoreIdx) {
            // the first failure ends the reads, also a failed wait
            if(result != VK_SUCCESS)
                break;
            result = table->vkGetSemaphoreCounterValue(
                table.device(),
                vkSemaphores[semaphoreIdx],
                &counters[semaphoreIdx]
            );
        }

        reached.clear();
        {
            std::scoped_lock const lock{*_mutexPtr};

            if(result == VK_SUCCESS) {
                // the reached waiters to the end, a semaphore outside of the wait arrays stays: its
                // await() woke the thread for the next wait
                auto const reachedRange = std::ranges::partition(
                    _pending,
                    [&](Pending const& pending) {
                        auto const semaphoreIdx = indexOf(pending.vkSemaphore);
                        return semaphoreIdx == vkSemaphores.size()
                            || pending.waiter->value > counters[semaphoreIdx];
                    }
                );
                for(auto const& pending : reachedRange)
                    reached.push_back(pending.waiter);
                _pending.erase(reachedRange.begin(), reachedRange.end());
            } else {
                JVK_STABLE_ERR(
                    true,
                    "the reactor failed to wait for the semaphores, got {}",
                    result
                ) {}
                error = cvk::make_error_code(result);
                fail(error, reached);
            }
        }

        post(reached);

        // the error ends the thread, later waiters get the error at once in await()
        if(error)
            return;
    }
}

void TimelineReactor::wake() const { _wake.signal(); }

void TimelineReactor::fail(
    std::error_code error,
    vector<cth::ref_wrapper<Waiter>>& waiters
) const {
    _error = error;

    for(auto const& pending : _pending) {
        pending.waiter->error = error;
        waiters.push_back(pending.waiter);
    }
    _pending.clear();
}

void TimelineReactor::post(std::span<cth::ref_wrapper<Waiter> const> waiters) {
    for(Waiter& waiter : waiters) {
        // the resumed caller can destroy the waiter at once: nothing touches it after the post
        auto const caller = waiter.caller;
        waiter.contextPtr->post([caller] { caller.resume(); });
    }
}
}
