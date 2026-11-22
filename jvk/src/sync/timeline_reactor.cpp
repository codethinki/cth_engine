#include "jvk/sync/timeline_reactor.hpp"

#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_error_code.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/io/log.hpp>

#include <exception>
#include <limits>
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
            !_waiters.empty(),
            "no waiter must be pending, got {} semaphores",
            _waiters.size()
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

    _waiters.clear();
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
            // the thread waits for the head per semaphore, a new head needs a new wait
            auto const it = _waiters.find(vkSemaphore);
            Waiter** linkPtr = it == _waiters.end() ? nullptr : &it->second;
            if(linkPtr == nullptr || waiter.value < (*linkPtr)->value)
                wake();

            // after the wake: a failed wake registers nothing
            if(linkPtr == nullptr)
                linkPtr = &_waiters[vkSemaphore];
            // sorted by value, behind the equal values
            while(*linkPtr != nullptr && (*linkPtr)->value <= waiter.value)
                linkPtr = &(*linkPtr)->nextPtr;
            waiter.nextPtr = *linkPtr;
            *linkPtr = &waiter;
            return;
        }

        waiter.error = _error;
        waiter.nextPtr = nullptr;
    }

    post(&waiter);
}

bool TimelineReactor::pending(VkSemaphore vkSemaphore) const {
    std::scoped_lock const lock{*_mutexPtr};
    return _waiters.contains(vkSemaphore);
}

void TimelineReactor::run(std::stop_token stop) {
    auto const table = _wake.core().deviceTable();

    // reused per round: the vectors keep their capacity
    vector<VkSemaphore> vkSemaphores{};
    vector<uint64_t> values{};
    std::error_code error{};

    while(true) {
        {
            std::scoped_lock const lock{*_mutexPtr};
            if(stop.stop_requested())
                return;

            vkSemaphores.assign({_wake.get()});
            values.assign({_wake.value() + 1});
            for(auto const& [vkSemaphore, head] : _waiters) {
                vkSemaphores.push_back(vkSemaphore);
                values.push_back(head->value);
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

        // the reached waiters of every semaphore in one list, sorted by value per semaphore
        Waiter* reached = nullptr;
        Waiter** reachedTailPtr = &reached;
        Waiter* failed = nullptr;
        {
            std::scoped_lock const lock{*_mutexPtr};

            for(auto it = _waiters.begin(); result == VK_SUCCESS && it != _waiters.end();) {
                auto& [vkSemaphore, head] = *it;

                uint64_t counter = 0;
                result = table->vkGetSemaphoreCounterValue(table.device(), vkSemaphore, &counter);
                if(result != VK_SUCCESS)
                    break;

                // the reached head part moves to the reached list, the list stays sorted
                Waiter* last = nullptr;
                for(auto* waiter = head; waiter != nullptr && waiter->value <= counter;
                    waiter = waiter->nextPtr)
                    last = waiter;
                if(last != nullptr) {
                    *reachedTailPtr = head;
                    reachedTailPtr = &last->nextPtr;
                    head = last->nextPtr;
                    last->nextPtr = nullptr;
                }

                // erase() keeps the other iterators valid
                if(head == nullptr)
                    _waiters.erase(it++);
                else
                    ++it;
            }

            if(result != VK_SUCCESS) {
                JVK_STABLE_ERR(
                    true,
                    "the reactor failed to wait for the semaphores, got {}",
                    result
                ) {}
                error = cvk::make_error_code(result);
                failed = fail(error);
            }
        }

        post(reached);

        // the error ends the thread, later waiters get the error at once in await()
        if(error) {
            post(failed);
            return;
        }
    }
}

void TimelineReactor::wake() const { _wake.signal(); }

auto TimelineReactor::fail(std::error_code error) const -> Waiter* {
    _error = error;

    Waiter* failed = nullptr;
    for(auto& [vkSemaphore, head] : _waiters)
        while(head != nullptr) {
            auto* const waiter = head;
            head = waiter->nextPtr;
            waiter->error = error;
            waiter->nextPtr = failed;
            failed = waiter;
        }
    _waiters.clear();

    return failed;
}

void TimelineReactor::post(Waiter* waiters) {
    while(waiters != nullptr) {
        // the resumed caller can destroy the waiter at once: nothing touches it after the post
        auto* const next = waiters->nextPtr;
        auto const caller = waiters->caller;
        waiters->contextPtr->post([caller] { caller.resume(); });
        waiters = next;
    }
}
}
