module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>
module cth.vk.contol;


import cth.io.log;

import cth.vk.present.os_window;
import cth.vk.exception;

namespace cth::vk {
void VkControl::terminate() {
    cth::log::msg<except::INFO>("terminating vk...");

    OSWindow::terminate();

    log::msg<except::INFO>("vk terminated");
    _initialized = false;
}
void VkControl::terminateVolk() { volkFinalize(); }
void VkControl::initVolk() {
    auto const result = volkInitialize();
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to load vulkan")
        throw result_exception{result, details->exception()};

    cth::log::msg<except::LOG>("loaded vulkan (volk)");
}

void VkControl::init() {
    cth::log::msg<except::INFO>("vk initializing...");
    initVolk();
    OSWindow::init();

    log::msg<except::INFO>("initialized vk");
    _initialized = true;
}


}
