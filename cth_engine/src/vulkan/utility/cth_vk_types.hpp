#pragma once
#include <volk.h>
#include <cth/pointer/not_null.hpp>
#include <gsl/pointers>

namespace cth::vk {
template<typename T>
// ReSharper disable once CppRedundantTemplateArguments
using not_null = not_null<gsl::owner<T>, VK_NULL_HANDLE>;
}
