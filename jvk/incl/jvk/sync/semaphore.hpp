#pragma once
#include "jvk/sync/timeline_reactor.hpp"

#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/constants.hpp"

#include <volk.h>

#include <cth/coro/context.hpp>
#include <cth/coro/counter_event.hpp>
#include <cth/coro/utility/fwd.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <coroutine>
#include <cstdint>
#include <system_error>
#include <utility>


namespace jvk {
class Core;
class Semaphore;

/**
 * awaiter of `Semaphore::wait()` and of `co_await` on a `Stage`
 * @details
 * - resumes the caller on its context, a failed wait throws in the caller
 * - holds the waiter of the reactor: the frame of the suspended caller holds it, no allocation
 */
class semaphore_awaiter : public cth::co::this_coro_awaiter_base {
public:
    constexpr semaphore_awaiter(Semaphore const& semaphore, uint64_t value) :
        _semaphore{semaphore},
        _waiter{.value = value} {}

    /**
     * @return the semaphore reached the value
     * @throws cvk::vk_result_exception result of `vkGetSemaphoreCounterValue()`
     */
    [[nodiscard]] bool await_ready() const;

    /**
     * hands the waiter to the reactor of the core, it posts the resume to the context of the caller
     * @details after the hand over nothing touches the awaiter: the resumed caller can end at once
     * @see `TimelineReactor::await()`
     */
    template<class Caller> requires cth::co::this_coro_compatible<Caller>
    void await_suspend(std::coroutine_handle<Caller> caller);

    /**
     * @throws cth::except::coro_exception reason: the wait failed, `code()` is
     *  `cvk::make_error_code()` of the result
     */
    void await_resume() const;

private:
    // a private helper: the reactor needs the complete jvk::Core, the template stays in the header
    /**
     * hands the waiter to the reactor of the core
     */
    void awaitReactor();

    cth::ref_wrapper<Semaphore const> _semaphore;
    TimelineReactor::Waiter _waiter;
};
}

namespace jvk {
/**
 * timeline semaphore with awaitable waits
 * @details
 * - holds a `cvk::TimelineSemaphore`, converts to it
 * - `wait()`: the reactor of the core waits for the gpu, no thread blocks
 * - `submitted()`: waits for the submit of a signal, the jvk submit paths call `markSubmitted()`
 * - a semaphore with a pending `submitted()` does not move, see `cth::co::counter_event`
 */
class Semaphore {
public:
    using State = cvk::TimelineSemaphore::State;

    /**
     * base constructor
     */
    explicit Semaphore(Core const&);

    /**
     * constructs and wraps
     * @details calls @ref Semaphore(Core const&), `wrap()`
     */
    Semaphore(Core const&, State const&);

    /**
     * constructs and creates
     * @details calls @ref Semaphore(Core const&), `create()` with the initial value
     */
    explicit Semaphore(Core const&, cvk::create_t, uint64_t initialValue = 0);

    /**
     * @note calls `optDestroy()`
     */
    ~Semaphore() { optDestroy(); }

    // the values given out before the wrap count as submitted
    /**
     * wraps the state
     * @details `submittedValue()` becomes `State::value`
     * @pre `Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the semaphore
     * @param initialValue of the semaphore, its counter and `submittedValue()`
     * @pre `Core::created()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateSemaphore()`
     */
    void create(uint64_t initialValue = 0);

    //TODO evaluate: destroy() with pending waits cancels them (operation_canceled), like asio
    /**
     * destroys and forgets
     * @pre `created()`
     * @pre the gpu does not use the semaphore
     * @pre no `wait()` or `submitted()` is pending
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
     * releases ownership, returns the state and forgets
     * @pre `created()`
     * @pre no `wait()` or `submitted()` is pending
     */
    State release();

    /**
     * marks the signal of the value as submitted
     * @details
     * - for the jvk submit paths, after the `vkQueueSubmit2()` of the signal
     * - posts the resume of every reached `submitted()` to the context of its caller
     * - thread safe against `submitted()` and `submittedValue()`
     * @pre `created()`
     * @pre `value > submittedValue()`
     * @pre no concurrent `markSubmitted()`
     */
    void markSubmitted(uint64_t value) const;

    /**
     * waits until a submit of a signal of the value reached `vkQueueSubmit2()`
     * @details
     * - not until the gpu reached the value
     * - resumes on the context of the caller
     * - marked -> no suspend
     * - no error: the jvk submit paths mark every signal value, also after an error of the submit
     * @pre `created()`
     * @see `markSubmitted()`, `wait()`
     */
    [[nodiscard]] constexpr cth::co::counter_event::awaiter submitted(uint64_t value) const {
        return _submitted.wait(value);
    }

    /**
     * waits until the semaphore reaches the value
     * @details
     * - resumes on the context of the caller
     * - reached -> no suspend
     * @pre `created()`
     */
    [[nodiscard]] constexpr semaphore_awaiter wait(uint64_t value) const {
        return semaphore_awaiter{*this, value};
    }

    // the counter forwarded: next(), value(), gpuValue(); signal() not: a host signal bypasses
    // markSubmitted()
    /**
     * advances the counter
     * @see `cvk::TimelineSemaphore::next()`
     */
    [[nodiscard]] constexpr uint64_t next() const { return _semaphore.next(); }

    /**
     * @see `cvk::TimelineSemaphore::value()`
     */
    [[nodiscard]] constexpr uint64_t value() const { return _semaphore.value(); }

    /**
     * @see `cvk::TimelineSemaphore::gpuValue()`
     */
    [[nodiscard]] uint64_t gpuValue() const { return _semaphore.gpuValue(); }

    /**
     * @return the largest value of `markSubmitted()`, the initial value before
     * @details thread safe
     */
    [[nodiscard]] uint64_t submittedValue() const { return _submitted.value(); }

    [[nodiscard]] constexpr bool created() const { return _semaphore.created(); }
    [[nodiscard]] constexpr VkSemaphore get() const { return _semaphore.get(); }
    [[nodiscard]] constexpr Core const& core() const { return *_core; }

    /**
     * the cvk semaphore
     * @attention the `wait()` overloads of `cvk::TimelineSemaphore` block the thread
     */
    constexpr operator cvk::TimelineSemaphore const&() const { return _semaphore; }

private:
    cth::ref_wrapper<Core const> _core;
    cvk::TimelineSemaphore _semaphore;

    // mutable: markSubmitted() is const like the submit paths that call it
    mutable cth::co::counter_event _submitted{}; ///< see `submittedValue()`

public:
    Semaphore(Semaphore const& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore const& other) = delete;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

    static constexpr void debug_check(Semaphore const&);
};
}

namespace jvk {
// in semaphore.hpp next to Semaphore: it is small, no own header
/**
 * wait or signal of a submit on a jvk semaphore
 * @details
 * - keeps the jvk semaphore: `Semaphore::submitted()` needs it
 * - converts to `cvk::SemaphoreSubmitInfo`
 */
struct SemaphoreSubmitInfo {
    cth::ref_wrapper<Semaphore const> semaphore;

    /**
     * wait -> the stages that wait, signal -> the stages that finish before the signal
     */
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    uint64_t value; ///< wait -> the value to wait for, signal -> the value to signal

    [[nodiscard]] constexpr operator cvk::SemaphoreSubmitInfo() const {
        return cvk::SemaphoreSubmitInfo{*semaphore, stages, value};
    }
};
}

namespace jvk {
constexpr void Semaphore::debug_check(Semaphore const& semaphore) {
    cvk::TimelineSemaphore::debug_check(semaphore);
}

template<class Caller> requires cth::co::this_coro_compatible<Caller>
void semaphore_awaiter::await_suspend(std::coroutine_handle<Caller> caller) {
    _waiter.caller = caller;
    _waiter.contextPtr = &caller.promise().context();
    awaitReactor();
}
}
