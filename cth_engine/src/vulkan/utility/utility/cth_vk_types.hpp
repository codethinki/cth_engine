#pragma once
#include <vulkan/vulkan.h>

namespace cth::vk {
template<typename T>
// ReSharper disable once CppRedundantTemplateArguments
using not_null = not_null<T, VK_NULL_HANDLE>;
}
