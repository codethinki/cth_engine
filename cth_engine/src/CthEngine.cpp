#include "CthEngine.hpp"

#include "vulkan/surface/CthWindow.hpp"

namespace cth {
void Engine::terminate() {
    cth::log::msg<except::INFO>("terminating engine...");

    Window::terminate();

    log::msg<except::INFO>("engine terminated");
    initialized = false;
}

void Engine::init() {
    cth::log::msg<except::INFO>("engine initializing...");

    Window::init();

    log::msg<except::INFO>("initialized engine");
    initialized = true;
}


}
