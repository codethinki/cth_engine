#pragma once
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/constants.hpp"

#include <volk.h>

#include <cth/coro/awaiters/base/wait_awaiter_base.hpp>
#include <cth/coro/context.hpp>
#include <cth/coro/utility/fwd.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <coroutine>
#include <cstdint>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>
#include <vector>


namespace jvk {
class Core;
class Semaphore;

//TODO review name: snake_case like the awaiters of cth (general type, like std)
/**
 * awaiter of `Semaphore::wait()` and of `co_await` on a `Stage`
 * @details resumes the caller on its context, a failed wait throws in the caller
 */
class semaphore_awaiter : public cth::co::this_coro_awaiter_base, public cth::co::wait_awaiter_base {
public:
    constexpr semaphore_awaiter(Semaphore const& semaphore, uint64_t value) :
        _semaphore{semaphore}, _value{value} {}

    /**
     * @return the semaphore reached the value
     * @throws cvk::vk_result_exception result of `vkGetSemaphoreCounterValue()`
     */
    [[nodiscard]] bool await_ready() const;

    /**
     * registers the resume of the caller with the reactor of the core
     * @details
     * - the callback of `cth::co::wait_awaiter_base::resume()`, posted to the context of the caller
     * - a failed wait: `await_resume()` throws `cth::except::coro_exception`, `code()` is `cvk::make_error_code()`
     *   of the result
     * @see `TimelineReactor::await()`
     */
    template<class Caller>
    requires cth::co::this_coro_compatible<Caller>
    void await_suspend(std::coroutine_handle<Caller> caller);

private:
    //TODO review a private helper: the reactor needs the complete jvk::Core, the template stays in the header
    /**
     * registers the callback with the reactor of the core
     */
    void awaitReactor(cth::co::context::await_func cb) const;

    cth::ref_wrapper<Semaphore const> _semaphore;
    uint64_t _value;
};

//TODO review name: snake_case like semaphore_awaiter
/**
 * awaiter of `Semaphore::submitted()`
 * @details resumes the caller on its context
 */
class submitted_awaiter : public cth::co::this_coro_awaiter_base {
public:
    constexpr submitted_awaiter(Semaphore const& semaphore, uint64_t value) :
        _semaphore{semaphore}, _value{value} {}

    /**
     * @return `Semaphore::submittedValue()` reached the value
     */
    [[nodiscard]] bool await_ready() const;

    /**
     * registers the resume of the caller with the semaphore
     * @details resumes on the context of the caller
     * @see `Semaphore::awaitSubmitted()`
     */
    template<class Caller>
    requires cth::co::this_coro_compatible<Caller>
    void await_suspend(std::coroutine_handle<Caller> caller);

    //TODO review no error: the jvk submit paths mark every signal value, also after an error of the submit
    constexpr void await_resume() const noexcept {}

private:
    cth::ref_wrapper<Semaphore const> _semaphore;
    uint64_t _value;
};
}

namespace jvk {
/**
 * timeline semaphore with awaitable waits
 * @details
 * - holds a `cvk::TimelineSemaphore`, converts to it
 * - `wait()`: the reactor of the core waits for the gpu, no thread blocks
 * - `submitted()`: waits for the submit of a signal, the jvk submit paths call `markSubmitted()`
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

    //TODO review the submitted value of a wrap: State::value, the values given out before the wrap count as submitted
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
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     * @pre no `wait()` or `submitted()` is pending
     */
    State release();

    //TODO review @pre rising values, like the signal values of a timeline semaphore
    /**
     * marks the signal of the value as submitted
     * @details
     * - for the jvk submit paths, after the `vkQueueSubmit2()` of the signal
     * - posts the callbacks of `awaitSubmitted()` up to the value to the workers of the core
     * - thread safe
     * @pre `created()`
     * @pre `value > submittedValue()`
     */
    void markSubmitted(uint64_t value) const;

    /**
     * waits until a submit of a signal of the value reached `vkQueueSubmit2()`
     * @details
     * - not until the gpu reached the value
     * - resumes on the context of the caller
     * - marked -> no suspend
     * @pre `created()`
     * @see `markSubmitted()`, `wait()`
     */
    [[nodiscard]] constexpr submitted_awaiter submitted(uint64_t value) const {
        return submitted_awaiter{*this, value};
    }

    //TODO review like TimelineReactor::await(): marked -> posts the callback at once, no bool result
    /**
     * calls the callback on a worker of the core once `markSubmitted()` reached the value
     * @details
     * - the callback gets an empty `std::error_code`
     * - marked -> posts the callback at once
     * - thread safe
     * @pre `created()`
     * @see `submitted()`
     */
    void awaitSubmitted(uint64_t value, cth::co::context::await_func cb) const;

    /**
     * waits until the semaphore reaches the value
     * @details
     * - resumes on the context of the caller
     * - reached -> no suspend
     * @pre `created()`
     */
    [[nodiscard]] constexpr semaphore_awaiter wait(uint64_t value) const { return semaphore_awaiter{*this, value}; }

    //TODO review the counter forwarded: next(), value(), gpuValue(); signal() not: a host signal bypasses
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
    [[nodiscard]] uint64_t submittedValue() const;

    [[nodiscard]] constexpr bool created() const { return _semaphore.created(); }
    [[nodiscard]] constexpr VkSemaphore get() const { return _semaphore.get(); }
    [[nodiscard]] constexpr Core const& core() const { return *_core; }

    /**
     * the cvk semaphore
     * @attention the `wait()` overloads of `cvk::TimelineSemaphore` block the thread
     */
    constexpr operator cvk::TimelineSemaphore const&() const { return _semaphore; }

private:
    /**
     * callback of `awaitSubmitted()`
     */
    struct SubmittedWaiter {
        uint64_t value;
        cth::co::context::await_func cb;
    };

    /**
     * the submitted value with its waiters
     */
    struct Submitted {
        std::mutex mutex{};

        uint64_t value = 0; ///< see `submittedValue()`

        std::vector<SubmittedWaiter> waiters{}; ///< the callbacks of `awaitSubmitted()` above `Submitted::value`
    };

    cth::ref_wrapper<Core const> _core;
    cvk::TimelineSemaphore _semaphore;

    //TODO review behind a std::unique_ptr: the mutex is not movable, the moves stay = default
    std::unique_ptr<Submitted> _submittedPtr;

public:
    Semaphore(Semaphore const& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore const& other) = delete;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

    static constexpr void debug_check(Semaphore const&);
};
}

namespace jvk {
//TODO review name: cvk::SemaphoreSubmitInfo with the jvk semaphore and a fixed value
//TODO review in semaphore.hpp next to Semaphore: it is small, no own header
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

namespace jvk::dev {
//TODO review name post_to
/**
 * callback that posts the callback to the context
 * @details the reactor and `Semaphore::markSubmitted()` call on a worker, the awaiters resume on the context of the
 *  caller
 */
[[nodiscard]] inline cth::co::context::await_func post_to(
    cth::co::context const& context,
    cth::co::context::await_func cb
) {
    return [&context, cb = std::move(cb)](std::error_code error) mutable {
        context.post([cb = std::move(cb), error]() mutable { cb(error); });
    };
}
}

namespace jvk {
constexpr void Semaphore::debug_check(Semaphore const& semaphore) { cvk::TimelineSemaphore::debug_check(semaphore); }

template<class Caller>
requires cth::co::this_coro_compatible<Caller>
void semaphore_awaiter::await_suspend(std::coroutine_handle<Caller> caller) {
    awaitReactor(dev::post_to(caller.promise().context(), resume(caller)));
}

template<class Caller>
requires cth::co::this_coro_compatible<Caller>
void submitted_awaiter::await_suspend(std::coroutine_handle<Caller> caller) {
    _semaphore->awaitSubmitted(
        _value,
        dev::post_to(caller.promise().context(), [caller](std::error_code) { caller.resume(); })
    );
}
}
