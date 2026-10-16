#pragma once
// ReSharper disable CppUnusedIncludeDirective
#include "constant/debug_constants.hpp"
// ReSharper restore CppUnusedIncludeDirective

#include <volk.h>

#include <cth/constants.hpp>

namespace cvk {
struct create_t {};

static constexpr create_t create{};
}

namespace cvk::constants {

constexpr bool ENABLE_VALIDATION_LAYERS = DEBUG_MODE;

constexpr auto COMPILATION_MODE = cth::COMPILATION_MODE;

} // namespace cvk::constants
