module;
#include "cth/io/io_log.hpp"
export module cth.engine.control;

import cth.vk.init;

import cth.io.log;

export namespace cth::vk {
class EngineControl {
public:
    EngineControl() = delete;

    static void init();
    static void terminate();

private:
    inline static bool _initialized = false;
};
void EngineControl::init() {
    CTH_CRITICAL(_initialized, "cannot initialize active engine") {}
    cth::log::msg<except::INFO>("initializing engine...");
    VkControl::init();
    _initialized = true;
    cth::log::msg<except::INFO>("initialized engine");

}
void EngineControl::terminate() {
    CTH_CRITICAL(!_initialized, "cannot terminate uninitialized engine") {}
    VkControl::terminate();
    _initialized = false;

    cth::log::msg<except::INFO>("terminated engine");
}
}
