#include "jvk/base/jvk.hpp"

#include "jvk/utility/vk_exceptions.hpp"

#include <cth/io/log.hpp>


namespace jvk {
void Jvk::terminate() {
    cth::log::msg<except::INFO>("terminating jvk...");

    terminateVolk();

    log::msg<except::INFO>("jvk terminated");
    _initialized = false;
}

void Jvk::terminateVolk() { volkFinalize(); }

void Jvk::initVolk() {
    auto const result = volkInitialize();
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to load vulkan")
    throw vk_result_exception{result, details->exception()};

    log::msg<except::LOG>("loaded vulkan (volk)");
}

void Jvk::init() {
    log::msg<except::INFO>("engine initializing...");
    initVolk();

    log::msg<except::INFO>("initialized engine");
    _initialized = true;
}


}
