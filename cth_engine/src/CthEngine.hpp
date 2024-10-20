#pragma once

namespace cth::vk {

class Engine {
public:
    static void init();
    static void terminate();

private:
    inline static bool _initialized = false;

    static void terminateVolk();
    static void initVolk();
public:
    [[nodiscard]] static bool initialized() { return _initialized; }
};

} // namespace cth
