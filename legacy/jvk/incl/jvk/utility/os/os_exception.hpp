#pragma once

#include <cth/exception.hpp>




namespace jvk::os {
class os_exception : public ::cth::except::default_exception {
public:
    explicit os_exception(
        std::string_view msg,
        cth::except::Severity const severity = except::ERR,
        std::source_location loc = {},
        std::stacktrace trace = {}
    )
        : default_exception{std::format("jvk_os: {}", msg), severity, std::move(loc), std::move(trace)} {}

};

}

#define JVK_STABLE_OS_THROW(expression, msg, ...)\
    CTH_STABLE_THROW_T(jvk::os::os_exception, expression, msg, __VA_ARGS__)


