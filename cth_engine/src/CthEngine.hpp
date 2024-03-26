#pragma once
#include <cth/cth_log.hpp>

namespace cth {

class Engine {
public:
    static void init();
    static void terminate();

private:
    inline static bool initialized = false;
};

} // namespace cth
