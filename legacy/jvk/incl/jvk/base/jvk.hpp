#pragma once

namespace jvk {
class Jvk {
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
}