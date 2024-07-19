#include "CthEngine.hpp"

#include "vulkan/surface/CthOSWindow.hpp"

namespace cth::vk {
void Engine::terminate() {
    cth::log::msg<except::INFO>("terminating engine...");

    OSWindow::terminate();

    log::msg<except::INFO>("engine terminated");
    _initialized = false;
}

void Engine::init() {
    cth::log::msg<except::INFO>("engine initializing...");

    OSWindow::init();

    log::msg<except::INFO>("initialized engine");
    _initialized = true;
}


}
