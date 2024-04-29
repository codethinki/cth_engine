#include "CthEngine.hpp"

#include <cth/cth_log.hpp>

#include "vulkan/surface/CthWindow.hpp"

namespace cth {
void Engine::terminate() {
    cth::log::msg<except::INFO>("terminating engine...");

    Window::terminate();

    log::msg<except::INFO>("engine terminated");
    _initialized = false;
}

void Engine::init() {
    cth::log::msg<except::INFO>("engine initializing...");

    Window::init();

    log::msg<except::INFO>("initialized engine");
    _initialized = true;
}


}
