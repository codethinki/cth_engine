#pragma once

namespace cth {

class Engine {
public:
    static void init();
    static void terminate();

private:
    inline static bool _initialized = false;

public:
    [[nodiscard]] static bool initialized() { return _initialized; }
};

} // namespace cth
