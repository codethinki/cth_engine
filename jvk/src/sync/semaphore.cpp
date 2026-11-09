#include "jvk/sync/semaphore.hpp"

#include "jvk/base/core.hpp"
#include "jvk/sync/timeline_reactor.hpp"

#include <cth/io/log.hpp>

#include <algorithm>
#include <utility>
#include <vector>


namespace jvk {
using await_func = cth::co::context::await_func;

bool semaphore_awaiter::await_ready() const { return _semaphore->gpuValue() >= _value; }

void semaphore_awaiter::awaitReactor(await_func cb) const {
    _semaphore->core().reactor().await(_semaphore->get(), _value, std::move(cb));
}

bool submitted_awaiter::await_ready() const { return _semaphore->submittedValue() >= _value; }


Semaphore::Semaphore(Core const& core) : _core{core}, _semaphore{core}, _submittedPtr{std::make_unique<Submitted>()} {}

Semaphore::Semaphore(Core const& core, State const& state) : Semaphore{core} { wrap(state); }

Semaphore::Semaphore(Core const& core, cvk::create_t, uint64_t initialValue) : Semaphore{core} {
    create(initialValue);
}

void Semaphore::wrap(State const& state) {
    optDestroy();

    _semaphore.wrap(state);

    std::scoped_lock const lock{_submittedPtr->mutex};
    _submittedPtr->value = state.value;
}

void Semaphore::create(uint64_t initialValue) {
    optDestroy();

    _semaphore.create(initialValue);

    std::scoped_lock const lock{_submittedPtr->mutex};
    _submittedPtr->value = initialValue;
}

void Semaphore::destroy() {
    debug_check(*this);
    CTH_CRITICAL(core().reactor().pending(get()), "no wait() must be pending") {}

    {
        std::scoped_lock const lock{_submittedPtr->mutex};
        CTH_CRITICAL(!_submittedPtr->waiters.empty(), "no submitted() must be pending") {}
        _submittedPtr->value = 0;
    }

    _semaphore.destroy();
}

Semaphore::State Semaphore::release() {
    debug_check(*this);
    CTH_CRITICAL(core().reactor().pending(get()), "no wait() must be pending") {}

    {
        std::scoped_lock const lock{_submittedPtr->mutex};
        CTH_CRITICAL(!_submittedPtr->waiters.empty(), "no submitted() must be pending") {}
        _submittedPtr->value = 0;
    }

    return _semaphore.release();
}

void Semaphore::markSubmitted(uint64_t value) const {
    debug_check(*this);

    std::vector<await_func> reached{};
    {
        std::scoped_lock const lock{_submittedPtr->mutex};
        CTH_CRITICAL(
            value <= _submittedPtr->value,
            "value must be > submittedValue(), got {} <= {}",
            value,
            _submittedPtr->value
        ) {}

        _submittedPtr->value = value;

        auto& waiters = _submittedPtr->waiters;
        auto const marked = std::ranges::partition(waiters, [value](SubmittedWaiter const& waiter) {
            return waiter.value > value;
        });
        for(auto& waiter : marked)
            reached.push_back(std::move(waiter.cb));
        waiters.erase(marked.begin(), marked.end());
    }

    auto const& workers = core().executor().context();
    for(auto& cb : reached)
        workers.post([cb = std::move(cb)]() mutable { cb({}); });
}

void Semaphore::awaitSubmitted(uint64_t value, await_func cb) const {
    debug_check(*this);

    {
        std::scoped_lock const lock{_submittedPtr->mutex};
        if(value > _submittedPtr->value) {
            _submittedPtr->waiters.push_back(SubmittedWaiter{value, std::move(cb)});
            return;
        }
    }

    core().executor().context().post([cb = std::move(cb)]() mutable { cb({}); });
}

uint64_t Semaphore::submittedValue() const {
    std::scoped_lock const lock{_submittedPtr->mutex};
    return _submittedPtr->value;
}
}
