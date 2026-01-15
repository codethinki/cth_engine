#pragma once
#include "jvk/surface/swapchain/swapchain_subpass_config.hpp"

namespace jly {
struct GraphicsCoreConfig {
    size_t framesInFlight;
    jvk::SwapchainSubpassConfig subpassConfig;

};
}