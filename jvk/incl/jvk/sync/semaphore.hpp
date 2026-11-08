#pragma once
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/utility/constants.hpp"

#include <volk.h>

#include <cth/coro/utility/fwd.hpp>
#include <cth/ptr/not_null.hpp>

#include <coroutine>
#include <cstdint>
#include <system_error>


namespace jvk {
class Core;
class Semaphore;

//TODO review name: snake_case like the awaiters of cth (general type, like std)
/**
 * awaiter of `Semaphore::wait()`
 * @details resumes the caller on its context
 */
class semaphore_awaiter : public cth::co::this_coro_awaiter_base {
public:
    constexpr semaphore_awaiter(Semaphore const& semaphore, uint64_t value) :
        _semaphorePtr{&semaphore}, _value{value} {}

    /**
     * @return the semaphore reached the value
     * @throws cvk::vk_result_exception result of `vkGetSemaphoreCounterValue()`
     */
    [[nodiscard]] bool await_ready() const;

    /**
     * registers the resume of the caller with the reactor of the core, see `TimelineReactor::await()`
     * @details resumes on the context of the caller
     */
    template<class Caller>
    requires cth::co::this_coro_compatible<Caller>
    void await_suspend(std::coroutine_handle<Caller> caller);

    //TODO review own await_resume(), not cth::co::wait_awaiter_base: jvk throws no cth::except::coro_exception
    /**
     * @throws jvk::jvk_exception reason: the wait of the reactor failed
     */
    void await_resume() const;

private:
    cth::not_null_ptr<Semaphore const> _semaphorePtr;
    uint64_t _value;

    /**
     * result of the wait, set by the callback
     */
    std::error_code _error{};
};
}

namespace jvk {
//TODO review public cvk::TimelineSemaphore base: cvk::SemaphoreSubmitInfo takes it as it is,
// the blocking cvk wait() overloads are hidden
/**
 * timeline semaphore with an awaitable wait
 * @details
 * - the reactor of the core waits, no thread blocks
 * - the counter and the `release()` of `cvk::TimelineSemaphore`
 */
class Semaphore : public cvk::TimelineSemaphore {
public:
    using State = cvk::TimelineSemaphore::State;

    /**
     * base constructor
     * @pre `Core::created()`
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

    /**
     * wraps the state
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * creates the semaphore
     * @param initialValue of the semaphore and the counter
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
     * @pre no `wait()` is pending
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * waits until the semaphore reaches the value
     * @details
     * - resumes on the context of the caller
     * - reached -> no suspend
     * @pre `created()`
     */
    [[nodiscard]] constexpr semaphore_awaiter wait(uint64_t value) const { return semaphore_awaiter{*this, value}; }

    //TODO review hides cvk::TimelineSemaphore::core(): the jvk core converts to the cvk core
    [[nodiscard]] constexpr Core const& core() const { return *_corePtr; }

private:
    cth::not_null_ptr<Core const> _corePtr;

public:
    Semaphore(Semaphore const& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore const& other) = delete;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

    static constexpr void debug_check(Semaphore const&);
};
}

namespace jvk {
constexpr void Semaphore::debug_check(Semaphore const& semaphore) { cvk::TimelineSemaphore::debug_check(semaphore); }
}
