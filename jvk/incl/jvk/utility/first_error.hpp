#pragma once
#include <cth/io/log.hpp>

#include <exception>
#include <mutex>
#include <utility>


namespace jvk::dev {
/**
 * the first error of the jobs and the scopes of an owner, the owner rethrows it
 * @details thread safe
 * @details `store()` keeps only the first error, the owner calls `clear()` only in its `destroy()`: a check with
 *  `operator bool()` and the `rethrow()` after it do not race
 */
class first_error {
public:
    /**
     * stores the error if it is the first
     */
    void store(std::exception_ptr errorPtr) noexcept {
        std::scoped_lock const lock{_mutex};
        if(!_errorPtr)
            _errorPtr = std::move(errorPtr);
    }

    /**
     * forgets the error, the next `store()` stores the first error again
     */
    void clear() noexcept {
        std::scoped_lock const lock{_mutex};
        _errorPtr = nullptr;
    }

    /**
     * rethrows the first error
     * @pre an error is stored
     */
    void rethrow() const {
        std::scoped_lock const lock{_mutex};
        CTH_CRITICAL(!_errorPtr, "rethrow() needs a stored error") {}
        std::rethrow_exception(_errorPtr);
    }

    /**
     * @return an error is stored
     */
    [[nodiscard]] explicit operator bool() const {
        std::scoped_lock const lock{_mutex};
        return static_cast<bool>(_errorPtr);
    }

private:
    mutable std::mutex _mutex{};
    std::exception_ptr _errorPtr = nullptr;
};
}
