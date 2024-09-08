#pragma once
#include<string_view>
#include<boost/pfr.hpp>

namespace cth::vk::fmt {


template<class T, size_t SvSize, size_t Params = boost::pfr::tuple_size_v<T>>
constexpr auto generate_format_string(char const (&type_name)[SvSize]) {

    static_assert(Params > 0 && Params <= 10, "more that 10 struct params are not supported");

    //example: Type{{{0}, {1}, {2}}}\0
    constexpr std::size_t totalSize = (SvSize - 1) + 2 + Params * 5 - 2 + 2 + 1;

    std::array<char, totalSize> result{};

    size_t pos = 0;
    for(; pos < SvSize - 1; ++pos) result[pos] = type_name[pos];
    result[pos++] = '{';
    result[pos++] = '{';

    for(size_t i = 0; i < Params; ++i) {
        result[pos++] = '{';
        result[pos++] = '0' + static_cast<char>(i);
        result[pos++] = '}';
        if(i == Params - 1) continue;
        result[pos++] = ',';
        result[pos++] = ' ';
    }
    result[pos++] = '}';
    result[pos++] = '}';

    return result;
}

template<class T>
[[nodiscard]] constexpr auto format_string() {
    if constexpr(std::same_as<T, VkSurfaceFormatKHR>) return generate_format_string<T>("VkSurfaceFormatKHR");
}
}
