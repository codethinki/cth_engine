#include "CthEngine.hpp"

#include "vulkan/surface/CthOSWindow.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
void Engine::terminate() {
    cth::log::msg<except::INFO>("terminating engine...");

    OSWindow::terminate();

    log::msg<except::INFO>("engine terminated");
    _initialized = false;
}
void Engine::terminateVolk() {
    volkFinalize();
}
void Engine::initVolk() {

    auto const result = volkInitialize();
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to load vulkan")
        throw result_exception{result, details->exception()};

    cth::log::msg<except::LOG>("loaded vulkan (volk)");
}

void Engine::init() {
    cth::log::msg<except::INFO>("engine initializing...");
    initVolk();
    OSWindow::init();

    log::msg<except::INFO>("initialized engine");
    _initialized = true;
}


}
