#pragma once
#include "jolly/utility/types.hpp"

#include <cth/coro/awaiters/native_handle_awaiter.hpp>

#include <memory>

namespace jvk {
class Fence;
}

namespace jly {
class Core;
}

namespace jly {

class Fence {
public:
    explicit Fence(Core const&);
    Fence(Core const&, bool signaled);
    ~Fence();

    void create(bool signaled);
    void destroy();

    void reset();

    [[nodiscard]] bool signaled() const;

    /**
     * blocks until fence is signaled or timeout is reached
     * @param timeout in nanoseconds
     * @return true if successful, false on timeout
     */
    [[nodiscard]] bool wait(size_t timeout) const;

    /**
     * blocks until fence is signaled
     */
    void wait() const;

    /**
     * blocks until fence is signaled, then resets or timeout is reached
     * @return true if successful, false on timeout
     * @details calls:
            - @ref wait(size_t) const
            - @ref reset()
     */
    [[nodiscard]] bool waitReset(size_t timeout);


    /**
     * blocks until fence is signaled, then resets
     */
    void waitReset();


    /**
     * @brief makes fence directly awaitable in coroutines
     * @attention requires @ref created()
     * @return Awaiter object for coroutine suspension
     * @note Usage: co_await fence.withContext(io_context);
     * @note Uses platform-specific native handle for efficient waiting
     */
    [[nodiscard]] cth::co::native_handle_awaiter operator co_await() const;

private:
    not_null<Core const*> _core;
    std::unique_ptr<jvk::Fence> _handle;

public:
    [[nodiscard]] jvk::Fence const& raw() const { return *_handle; }
    [[nodiscard]] bool created() const;

    Fence(Fence const& other) = delete;
    Fence& operator=(Fence const& other) = delete;
    Fence(Fence&& other) noexcept;
    Fence& operator=(Fence&& other) noexcept = default;
};
}
