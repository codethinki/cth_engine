#pragma once
#include "cvk/utility/vk_exceptions.hpp"

#include <volk.h>

#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>


namespace cvk::dev {
/**
 * @brief element type of a two-call query: the pointee of its last parameter
 */
template<class Fn>
struct enumerate_element {};

template<class Result, class... Params>
struct enumerate_element<Result (*)(Params...)> {
    using type = std::remove_pointer_t<std::tuple_element_t<sizeof...(Params) - 1, std::tuple<Params...>>>;
};

template<class Fn>
using enumerate_element_t = enumerate_element<Fn>::type;

/**
 * @brief max count and write rounds of @ref enumerate() while the result is VK_INCOMPLETE
 */
constexpr uint32_t MAX_QUERY_ATTEMPTS = 4;

/**
 * @brief calls a two-call query as fn(args..., &count, data), first for the count, then for the elements
 * @param what name of the elements for the error message
 * @param fn function pointer: a volk global or a VolkDeviceTable member
 * @param args arguments before the count
 * @details VkResult fn -> repeats both calls while the result is VK_INCOMPLETE,
 *  at most @ref MAX_QUERY_ATTEMPTS rounds,
 *  void fn -> one count and one write call
 * @return the elements, as many as the last call wrote
 * @throws cvk::vk_result_exception result of fn other than VK_SUCCESS or VK_INCOMPLETE
 * @throws cvk::vk_result_exception VK_INCOMPLETE in all @ref MAX_QUERY_ATTEMPTS rounds
 */
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
        for(uint32_t attempt = 0; attempt < MAX_QUERY_ATTEMPTS && result == VK_INCOMPLETE; ++attempt) {
            auto const countResult = fn(args..., &count, nullptr);
            CVK_RESULT_STABLE_THROW(countResult != VK_SUCCESS, countResult, "failed to query {}", what) {}
            // data() of an empty vector can be nullptr, a second count call could return a new count
            if(count == 0) return {};

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
