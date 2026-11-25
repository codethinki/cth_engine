#pragma once
#include "cvk/utility/vk_exceptions.hpp"

#include <volk.h>

#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>


namespace cvk::dev {
template<class Fn>
struct enumerate_element {};

template<class Result, class... Params>
struct enumerate_element<Result (*)(Params...)> {
    using type = std::remove_pointer_t<std::tuple_element_t<sizeof...(Params) - 1, std::tuple<Params
            ...>>
    >;
};

template<class Fn>
using enumerate_element_t = enumerate_element<Fn>::type;

constexpr uint32_t MAX_QUERY_ATTEMPTS = 4;

template<class Fn, class... Args>
[[nodiscard]] std::vector<enumerate_element_t<Fn>> enumerate(
    [[maybe_unused]] std::string_view what,
    Fn fn,
    Args const&... args
) {
    using element_t = enumerate_element_t<Fn>;

    std::vector<element_t> elements{};
    uint32_t count = 0;

    if constexpr(std::is_void_v<std::invoke_result_t<Fn, Args const&..., uint32_t*, element_t*>>) {
        fn(args..., &count, nullptr);
        elements.resize(count);
        fn(args..., &count, elements.data());
    } else {
        VkResult result = VK_INCOMPLETE;
        for(uint32_t i = 0; i < MAX_QUERY_ATTEMPTS && result == VK_INCOMPLETE; ++i) {
            auto const countResult = fn(args..., &count, nullptr);
            CVK_RESULT_STABLE_THROW(
                countResult != VK_SUCCESS,
                countResult,
                "failed to query {}",
                what
            ) {}
            // data() of an empty vector can be nullptr, a second count call could return a new count
            if(count == 0)
                return {};

            elements.resize(count);
            result = fn(args..., &count, elements.data());
        }
        CVK_RESULT_STABLE_THROW(
            result == VK_INCOMPLETE,
            result,
            "failed to query {}, the count changed on every attempt, got {} attempts",
            what,
            MAX_QUERY_ATTEMPTS
        ) {}
        CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to query {}", what) {}
    }

    elements.resize(count);
    return elements;
}
}
