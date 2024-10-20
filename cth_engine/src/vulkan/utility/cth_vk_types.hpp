#pragma once
#include <gsl/pointers>

#include <volk.h>

namespace cth::vk {
template<typename T>
// ReSharper disable once CppRedundantTemplateArguments
using not_null = not_null<gsl::owner<T>, VK_NULL_HANDLE>;
}
