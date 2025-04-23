#pragma once
#include <volk.h>
#include <gsl/pointers>

import cth.ptr.not_null;

namespace cth::vk {
template<typename T>
// ReSharper disable once CppRedundantTemplateArguments
using not_null = not_null<gsl::owner<T>, VK_NULL_HANDLE>;
}
