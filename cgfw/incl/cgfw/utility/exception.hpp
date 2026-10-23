#pragma once
#include <cth/exception.hpp>
#include <cth/io/log.hpp>

#include <format>
#include <source_location>
#include <stacktrace>
#include <string_view>
#include <utility>


namespace cgfw {
/**
 * @brief exception of cgfw, the message starts with "cgfw: "
 */
class cgfw_exception : public cth::except::default_exception {
    static constexpr auto PREFIX = "cgfw: ";

public:
    cgfw_exception(
        std::string_view msg,
        cth::except::Severity const severity,
        std::source_location loc,
        std::stacktrace trace
    ) : default_exception{
        std::format("{}{}", PREFIX, msg),
        severity,
        std::move(loc),
        std::move(trace)
    } {}

    explicit cgfw_exception(cth::except::default_exception defaultException)
        : default_exception{std::move(defaultException)} { prepend(PREFIX); }
};
}

#define CGFW_STABLE_THROW(expression, msg, ...) \
    CTH_STABLE_THROW_T(cgfw::cgfw_exception, expression, msg, __VA_ARGS__)
