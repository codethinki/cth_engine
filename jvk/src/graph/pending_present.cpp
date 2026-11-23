#include "jvk/graph/pending_present.hpp"


namespace jvk::dev {
void PresentState::present(uint64_t value, VkResult result) {
    if(result == VK_ERROR_OUT_OF_DATE_KHR)
        outOfDate = true;
    dropped = false;
    // the last write: Surface::acquire() reads the state after the wait for the value
    resolved.set(value);
}

void PresentState::drop(uint64_t value) {
    dropped = true;
    resolved.set(value);
}
}
