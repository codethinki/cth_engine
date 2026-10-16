#pragma once
#include <volk.h>

#include <cth/ptr/move_ptr.hpp>
#include <cth/ptr/not_null.hpp>

namespace cvk {
template<class T>
using vk_not_null = cth::not_null<T, VK_NULL_HANDLE>;
template<class T>
using not_null = cth::not_null<T>;

template<class T>
using move_ptr = cth::move_ptr<T>;

template<class T>
using unique_not_null = cth::unique_not_null<T>;

using CompilationMode = cth::CompilationMode;
}
