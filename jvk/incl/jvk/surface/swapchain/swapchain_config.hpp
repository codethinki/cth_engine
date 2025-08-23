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

    static void debug_check(SwapchainConfig const& config) {
        CTH_CRITICAL(
            config.imageAvailableSemaphores.size() != constants::FRAMES_IN_FLIGHT,
            "imageAvailableSemaphores.size() [{}] must equal frames in flight",
            config.imageAvailableSemaphores.size()
            ) {}
        CTH_CRITICAL(
            config.renderFinishedSemaphores.size() != constants::FRAMES_IN_FLIGHT,
            "renderFinishedSemaphores.size() [{}] must equal frames in flight",
            config.renderFinishedSemaphores.size()
            ) {}
        CTH_CRITICAL(config.subpassConfig.subpassLayouts.empty(),
            "subpassLayouts must not be empty, there must be at least one subpass") {}
    }
};
}