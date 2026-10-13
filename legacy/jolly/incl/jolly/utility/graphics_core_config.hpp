#pragma once
#include "jvk/surface/swapchain/swapchain_subpass_config.hpp"

namespace jly {
struct GraphicsCoreConfig {
    size_t framesInFlight;
    jvk::SwapchainSubpassConfig subpassConfig;

    static void debug_check(GraphicsCoreConfig const& config) {
        CTH_CRITICAL(config.framesInFlight <= 0, "frames in flight must be > 0") {}
    }
};
}
