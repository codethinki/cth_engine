#include "jvk/sync/semaphore.hpp"

#include "jvk/base/core.hpp"
#include "jvk/sync/timeline_reactor.hpp"

#include <cth/io/log.hpp>

#include <utility>


namespace jvk {
using await_func = cth::co::context::await_func;

bool semaphore_awaiter::await_ready() const { return _semaphore->gpuValue() >= _value; }

void semaphore_awaiter::awaitReactor(await_func cb) const {
    _semaphore->core().reactor().await(_semaphore->get(), _value, std::move(cb));
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
