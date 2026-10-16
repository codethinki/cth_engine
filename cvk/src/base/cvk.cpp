#include "cvk/base/cvk.hpp"

#include "cvk/utility/vk_exceptions.hpp"

#include <cth/io/log.hpp>


namespace cvk {
void Cvk::terminate() {
    log::msg<except::INFO>("terminating cvk...");

    terminateVolk();

    log::msg<except::INFO>("cvk terminated");
    _initialized = false;
}

void Cvk::terminateVolk() { volkFinalize(); }

void Cvk::initVolk() {
    auto const result = volkInitialize();
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to load vulkan") {}

    log::msg<except::LOG>("loaded vulkan (volk)");
}

void Cvk::init() {
    log::msg<except::INFO>("cvk initializing...");
    initVolk();

    log::msg<except::INFO>("cvk initialized");
    _initialized = true;
}
}
