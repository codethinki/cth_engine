#include "jvk/graph/pending_present.hpp"

#include "cvk/utility/vk_exceptions.hpp"

#include <exception>
#include <utility>


namespace jvk::dev {
void PresentState::present(uint64_t value, VkResult result, std::exception_ptr error) {
    if(result == VK_ERROR_OUT_OF_DATE_KHR)
        outOfDate = true;
    if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
        // a result without an exception of the present: a failed construction keeps its own error,
        // the image resolves anyway
        if(error == nullptr) {
            try { throw cvk::vk_result_exception{result, "failed to present the image"}; }
            catch(...) { error = std::current_exception(); }
        }
        this->error = std::move(error);
    }
    dropped = false;
    // the last write: Surface::acquire() reads the state after the wait for the value
    resolved.set(value);
}

void PresentState::drop(uint64_t value) {
    dropped = true;
    resolved.set(value);
}
}
