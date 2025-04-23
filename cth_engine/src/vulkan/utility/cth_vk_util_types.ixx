module;
#include "lib/gsl.hpp"
#include "lib/volk.hpp"
export module cth.vk.util.types;


import cth.ptr.not_null;

export namespace cth::vk {
template<typename T>
// ReSharper disable once CppRedundantTemplateArguments
using not_null = not_null<gsl::owner<T>, VK_NULL_HANDLE>;
}