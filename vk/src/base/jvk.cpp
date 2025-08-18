#include "vk/base/jvk.hpp"

#include "vk/utility/cth_vk_exceptions.hpp"



namespace cth::vk {
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
        throw result_exception{result, details->exception()};

    cth::log::msg<except::LOG>("loaded vulkan (volk)");
}

void Jvk::init() {
    cth::log::msg<except::INFO>("engine initializing...");
    initVolk();

    log::msg<except::INFO>("initialized engine");
    _initialized = true;
}


}
