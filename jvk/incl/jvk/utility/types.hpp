#pragma once
#include <volk.h>
#include <cth/pointer/move_ptr.hpp>
#include <cth/pointer/not_null.hpp>
#include <gsl/pointers>

namespace jvk {
template<class T>
// ReSharper disable once CppRedundantTemplateArguments
using vk_not_null = cth::not_null<gsl::owner<T>, VK_NULL_HANDLE>;
template<class T>
using not_null = cth::not_null<T>;

template<class T>
using move_ptr = cth::move_ptr<T>;

template<class T>
using unique_not_null = cth::unique_not_null<T>;

using CompilationMode = cth::CompilationMode;
}
