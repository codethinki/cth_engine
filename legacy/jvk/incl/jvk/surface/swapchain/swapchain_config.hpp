#pragma once
#include "swapchain_subpass_config.hpp"

#include "jvk/utility/constants.hpp"

#include <vector>

namespace jvk {
class Semaphore;
}

namespace jvk {


struct SwapchainConfig {
    std::vector<Semaphore const*> imageAvailableSemaphores;
    std::vector<Semaphore const*> renderFinishedSemaphores;

    SwapchainSubpassConfig subpassConfig;

    [[nodiscard]] constexpr size_t framesInFlight() const { return imageAvailableSemaphores.size(); }

    static void debug_check(SwapchainConfig const& config) {
        CTH_CRITICAL(
            config.imageAvailableSemaphores.size() != config.renderFinishedSemaphores.size(),
            "imageAvailableSemaphores.size() [{}] must equal renderFinishedSemaphores.size() [{}]"
            " (and be equal to frames in flight)",
            config.imageAvailableSemaphores.size(),
            config.renderFinishedSemaphores.size()
        ) {}

        CTH_CRITICAL(
            config.subpassConfig.subpassLayouts.empty(),
            "subpassLayouts must not be empty, there must be at least one subpass"
        ) {}
    }
};
}
