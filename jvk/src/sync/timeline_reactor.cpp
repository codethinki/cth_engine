#include "jvk/sync/timeline_reactor.hpp"

#include "jvk/utility/exception.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_error_code.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/scheduler.hpp>
#include <cth/io/log.hpp>

#include <algorithm>
#include <exception>
#include <functional>
#include <limits>
#include <utility>


namespace jvk {
using std::vector;
using await_func = cth::co::context::await_func;

TimelineReactor::TimelineReactor(cvk::Core const& core, cth::co::scheduler const& workers) :
    _core{core}, _workers{workers} {}

void TimelineReactor::create() {
    cvk::Core::debug_check(*_core);

    optDestroy();

    _wakeOpt.emplace(*_core, cvk::create);
    _error = {};

    try {
        _thread = std::jthread{[this](std::stop_token stop) { run(std::move(stop)); }};
    }
    catch(...) {
        _wakeOpt.reset();
        throw;
    }
}

void TimelineReactor::destroy() {
    CTH_CRITICAL(!created(), "reactor must be created") {}

    {
        std::scoped_lock const lock{_mutex};
        CTH_CRITICAL(!_waiters.empty(), "no callback must be pending, got {} semaphores", _waiters.size()) {}

        _thread.request_stop();

        // a failed wait ended the thread already, the wake does not matter then
        try {
            wake();
        }
        catch(cvk::vk_result_exception const& e) {
            JVK_STABLE_ERR(!_error, "failed to wake the reactor in destroy(), got {}", e.vkResult()) {}
        }
    }

    _thread.join();
    _thread = {};

    _waiters.clear();
    _error = {};
    _wakeOpt.reset();
}

void TimelineReactor::await(VkSemaphore vkSemaphore, uint64_t value, await_func cb) const {
    CTH_CRITICAL(!created(), "reactor must be created") {}
    CTH_CRITICAL(vkSemaphore == VK_NULL_HANDLE, "the semaphore must not be VK_NULL_HANDLE") {}

    std::error_code error{};
    {
        std::scoped_lock const lock{_mutex};

        if(!_error) {
            // the thread waits for the smallest value per semaphore, a new smallest value needs a new wait
            auto const it = _waiters.find(vkSemaphore);
            bool const first = it == _waiters.end();
            if(first || value < it->second.front().value) wake();

            // after the wake: a failed wake registers nothing
            auto& waiters = first ? _waiters[vkSemaphore] : it->second;
            waiters.push_back(Waiter{value, std::move(cb)});
            std::ranges::push_heap(waiters, std::ranges::greater{}, &Waiter::value);
            return;
        }

        error = _error;
    }

    post(std::move(cb), error);
}

bool TimelineReactor::pending(VkSemaphore vkSemaphore) const {
    std::scoped_lock const lock{_mutex};
    return _waiters.contains(vkSemaphore);
}

void TimelineReactor::run(std::stop_token stop) {
    auto const table = _core->deviceTable();

    vector<VkSemaphore> vkSemaphores{};
    vector<uint64_t> values{};
    vector<await_func> reached{};
    vector<await_func> failed{};
    std::error_code error{};

    while(true) {
        {
            std::scoped_lock const lock{_mutex};
            if(stop.stop_requested()) return;

            vkSemaphores.assign({_wakeOpt->get()});
            values.assign({_wakeOpt->value() + 1});
            for(auto const& [vkSemaphore, waiters] : _waiters) {
                vkSemaphores.push_back(vkSemaphore);
                values.push_back(waiters.front().value);
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
        auto result = table->vkWaitSemaphores(table.device(), &info, std::numeric_limits<uint64_t>::max());

        {
            std::scoped_lock const lock{_mutex};

            for(auto it = _waiters.begin(); result == VK_SUCCESS && it != _waiters.end();) {
                auto& [vkSemaphore, waiters] = *it;

                uint64_t counter = 0;
                result = table->vkGetSemaphoreCounterValue(table.device(), vkSemaphore, &counter);
                if(result != VK_SUCCESS) break;

                while(!waiters.empty() && waiters.front().value <= counter) {
                    std::ranges::pop_heap(waiters, std::ranges::greater{}, &Waiter::value);
                    reached.push_back(std::move(waiters.back().cb));
                    waiters.pop_back();
                }

                // erase() keeps the other iterators valid
                if(waiters.empty()) _waiters.erase(it++);
                else ++it;
            }

            if(result != VK_SUCCESS) {
                JVK_STABLE_ERR(true, "the reactor failed to wait for the semaphores, got {}", result) {}
                error = cvk::make_error_code(result);
                failed = fail(error);
            }
        }

        for(auto& cb : reached)
            post(std::move(cb), {});
        reached.clear();

        // the error ends the thread, later callbacks get the error at once in await()
        if(error) {
            for(auto& cb : failed)
                post(std::move(cb), error);
            return;
        }
    }
}

void TimelineReactor::wake() const { _wakeOpt->signal(); }

vector<await_func> TimelineReactor::fail(std::error_code error) const {
    _error = error;

    vector<await_func> callbacks{};
    for(auto& [vkSemaphore, waiters] : _waiters)
        for(auto& waiter : waiters)
            callbacks.push_back(std::move(waiter.cb));
    _waiters.clear();

    return callbacks;
}

void TimelineReactor::post(await_func cb, std::error_code error) const {
    _workers->post([cb = std::move(cb), error]() mutable { cb(error); });
}
}
