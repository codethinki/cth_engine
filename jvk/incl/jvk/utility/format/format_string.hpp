#pragma once
#include <string_view>
#include <volk.h>
#include <boost/pfr.hpp>
#include <cth/macro.hpp>

namespace jvk::fmt {

namespace dev {
    inline cval size_t number_chars(size_t params) {
        cxpr static size_t BASE = 10;


        size_t n = 0;
        size_t counted = 0;

        for(size_t i = 1, power = BASE; params > 0; i++, power *= BASE) {
            auto const numbers = std::min(power - counted, params);
            counted += numbers;

            params -= numbers;

            n += i * numbers;
        }

        return n;
    }

    template<size_t N>
    [[nodiscard]] cxpr size_t write_num(std::array<char, N>& buffer, size_t nr, size_t offset) {
        cxpr static size_t BASE = 10;

        //write nr lower first
        size_t digits = 0;
        while(digits == 0 || nr != 0) {
            buffer[offset + digits] = '0' + static_cast<char>(nr % BASE);
            digits++;

            nr /= BASE;
        }

        //reverse
        for(size_t i = 0; i < digits / 2; i++)
            std::swap(buffer[offset + i], buffer[offset + digits - i - 1]);
        return digits;
    }

}



template<class T, size_t SvSize, size_t Params = boost::pfr::tuple_size_v<T>>
cxpr auto generate_format_string(char const (&type_name)[SvSize]) {

    //example: Type{{{0}, {1}, {2}}}\0
    cxpr size_t bracketChars = 2 + Params * 2 + 2;
    cxpr size_t spacerSize = 2;
    cxpr size_t spacerChars = (Params - 1) * spacerSize;
    cxpr size_t fieldChars = dev::number_chars(Params) + bracketChars + spacerChars;
    cxpr size_t terminatorChars = 1;
    cxpr size_t typeChars = SvSize - 1;
    cxpr size_t totalChars = typeChars + fieldChars + terminatorChars;

    std::array < char, totalChars > result{};

    size_t pos = 0;
    for(; pos < typeChars; pos++) result[pos] = type_name[pos];


    result[pos++] = '{';
    result[pos++] = '{';

    for(size_t i = 0; i < Params; ++i) {
        result[pos++] = '{';
        pos += dev::write_num(result, i, pos);
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
    else if constexpr(std::same_as<T, VkExtent2D>) return generate_format_string<T>("VkExtent2D");
    else static_assert(false, "fuck");
}
}