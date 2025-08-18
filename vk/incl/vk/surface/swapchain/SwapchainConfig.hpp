#pragma once
#include "vk/utility/cth_constants.hpp"

namespace cth::vk {
class Semaphore;
}

namespace cth::vk {


struct SwapchainConfig {
    std::vector<Semaphore const*> imageAvailableSemaphores;
    std::vector<Semaphore const*> renderFinishedSemaphores;

    static void debug_check(SwapchainConfig const& config) {
        CTH_CRITICAL(
            config.imageAvailableSemaphores.size() != constants::FRAMES_IN_FLIGHT,
            "imageAvailableSemaphores.size() [{}] must equal frames in flight",
            config.imageAvailableSemaphores.size()
        ){}
        CTH_CRITICAL(
            config.renderFinishedSemaphores.size() != constants::FRAMES_IN_FLIGHT,
            "renderFinishedSemaphores.size() [{}] must equal frames in flight",
            config.renderFinishedSemaphores.size()
        ){}

    }
};
}
