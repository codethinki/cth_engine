#pragma once

// ReSharper disable CppUnusedIncludeDirective
#include "utility/cth_vk_format.hpp"
#include "utility/cth_vk_types.hpp"
#include "utility/cth_vk_exceptions.hpp"
// ReSharper restore CppUnusedIncludeDirective

#include <algorithm>


namespace cth::vk::utils {

[[nodiscard]] inline std::vector<char const*> to_c_str_vector(std::span<std::string const> const& str_vec) {
    std::vector<char const*> charVec(str_vec.size());
    std::ranges::transform(str_vec, charVec.begin(), [](std::string const& str) { return str.c_str(); });

    return charVec;
}
} // namespace cth
