#include "jvk/sync/semaphore.hpp"

#include "jvk/base/core.hpp"
#include "jvk/sync/timeline_reactor.hpp"

#include <cth/coro/exception.hpp>
#include <cth/io/log.hpp>

#include <utility>


namespace jvk {
bool semaphore_awaiter::await_ready() const { return _semaphore->gpuValue() >= _waiter.value; }

void semaphore_awaiter::awaitReactor() {
    // the last access to the awaiter: the reactor can post the resume before await() returns
    _semaphore->core().reactor().await(_semaphore->get(), _waiter);
}

void semaphore_awaiter::await_resume() const {
    CTH_CORO_CODE_STABLE_THROW(
        _waiter.error,
        _waiter.error,
        "wait failed, got {}",
        _waiter.error.message()
    ) {}
}

Semaphore::Semaphore(Core const& core) : _core{core}, _semaphore{core} {}

Semaphore::Semaphore(Core const& core, State const& state) : Semaphore{core} { wrap(state); }

Semaphore::Semaphore(Core const& core, cvk::create_t, uint64_t initialValue) : Semaphore{core} {
    create(initialValue);
}

void Semaphore::wrap(State const& state) {
    optDestroy();

    _semaphore.wrap(state);
    _submitted = cth::co::counter_event{state.value};
}

void Semaphore::create(uint64_t initialValue) {
    optDestroy();

    _semaphore.create(initialValue);
    _submitted = cth::co::counter_event{initialValue};
}

void Semaphore::destroy() {
    debug_check(*this);
    CTH_CRITICAL(core().reactor().pending(get()), "no wait() must be pending") {}
    // the assignment checks that no submitted() is pending
    _submitted = cth::co::counter_event{};

    _semaphore.destroy();
}

Semaphore::State Semaphore::release() {
    debug_check(*this);
    CTH_CRITICAL(core().reactor().pending(get()), "no wait() must be pending") {}
    // the assignment checks that no submitted() is pending
    _submitted = cth::co::counter_event{};

    return _semaphore.release();
}

void Semaphore::markSubmitted(uint64_t value) const {
    debug_check(*this);
    _submitted.set(value);
}
}
