#pragma once
#include <cth/exception.hpp>
#include <cth/io/log.hpp>

#include <format>
#include <source_location>
#include <stacktrace>
#include <string_view>
#include <utility>


namespace jvk {
/**
 * exception of jvk
 * @details the message starts with "jvk: "
 */
class jvk_exception : public cth::except::default_exception {
    static constexpr auto PREFIX = "jvk: ";

public:
    jvk_exception(
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

    explicit jvk_exception(cth::except::default_exception defaultException)
        : default_exception{std::move(defaultException)} { prepend(PREFIX); }
};
}

#define JVK_STABLE_THROW(expression, msg, ...) \
    CTH_STABLE_THROW_T(jvk::jvk_exception, expression, msg, __VA_ARGS__)

#define JVK_STABLE_ERR(expression, msg, ...) \
    CTH_STABLE_ERR_T(jvk::jvk_exception, expression, msg, __VA_ARGS__)

#define JVK_STABLE_WARN(expression, msg, ...) \
    CTH_STABLE_WARN_T(jvk::jvk_exception, expression, msg, __VA_ARGS__)
