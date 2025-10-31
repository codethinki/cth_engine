#pragma once
#include <cth/pointer/not_null.hpp>

namespace jly {
template<class T> using not_null = cth::not_null<T>;
}

namespace jly {
inline constexpr auto create = [] {};
using create_t = decltype(create);
}
