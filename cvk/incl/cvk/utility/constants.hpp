#pragma once
// ReSharper disable CppUnusedIncludeDirective
#include "constant/debug_constants.hpp"
// ReSharper restore CppUnusedIncludeDirective

#include <volk.h>

namespace cvk {
struct create_t {};

inline constexpr create_t create{};
}

namespace cvk::constants {
constexpr bool ENABLE_VALIDATION_LAYERS = DEBUG_MODE;
}
