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
    JVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to load vulkan") {}

    log::msg<except::LOG>("loaded vulkan (volk)");
}

void Jvk::init() {
    log::msg<except::INFO>("engine initializing...");
    initVolk();

    log::msg<except::INFO>("initialized engine");
    _initialized = true;
}


}
