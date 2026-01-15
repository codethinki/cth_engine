#pragma once
#include <cth/ptr/not_null.hpp>

namespace jly {
template<class T> using not_null = cth::not_null<T>;
}

namespace jly {
struct create_t {};
inline constexpr create_t create{};
}
