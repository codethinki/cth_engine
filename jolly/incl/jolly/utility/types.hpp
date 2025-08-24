#pragma once
namespace jly {
inline constexpr auto create = [] {};
using create_t = decltype(create);
}
