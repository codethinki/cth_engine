#pragma once
#include "cvk/utility/vk_exceptions.hpp"

#include <volk.h>

#include <concepts>
#include <exception>
#include <mutex>
#include <utility>


namespace jvk::dev {
/**
 * the first error of the jobs and the scopes of an owner, the owner rethrows it
 * @details thread safe
 */
class first_error {
public:
    /**
     * stores the error if it is the first
     */
    void store(std::exception_ptr errorPtr) noexcept {
        std::scoped_lock const lock{_mutex};
        if(!_errorPtr) _errorPtr = std::move(errorPtr);
    }

    /**
     * forgets the error, the next `store()` stores the first error again
     */
    void clear() noexcept {
        std::scoped_lock const lock{_mutex};
        _errorPtr = nullptr;
    }

    /**
     * rethrows the first error, no error -> nothing
     */
    void rethrow() const {
        std::scoped_lock const lock{_mutex};
        if(_errorPtr) std::rethrow_exception(_errorPtr);
    }

private:
    mutable std::mutex _mutex{};
    std::exception_ptr _errorPtr = nullptr;
};

/**
 * the empty submit after a failed submit: the waiters of its signals go on
 * @details `VK_ERROR_DEVICE_LOST` -> stores the error, the waits end on their own
 * @param submit runs the empty submit
 * @param[out] error gets a device loss
 * @throws cvk::vk_result_exception other results of the submit: the waiters would hang
 */
template<std::invocable Submit>
void fallback_submit(Submit&& submit, first_error& error) {
    try {
        std::forward<Submit>(submit)();
    }
    catch(cvk::vk_result_exception const& e) {
        if(e.vkResult() != VK_ERROR_DEVICE_LOST) throw;
        error.store(std::current_exception());
    }
}
}
