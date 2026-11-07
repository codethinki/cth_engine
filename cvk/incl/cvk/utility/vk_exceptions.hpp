#pragma once
#include "format.hpp"

#include <volk.h>

#include <cth/exception.hpp>
#include <cth/io/log.hpp>


namespace cvk {
/**
 * exception of cvk
 * @details the message starts with "cvk: "
 */
class cvk_exception : public cth::except::default_exception {
    static constexpr auto PREFIX = "cvk: ";

public:
    cvk_exception(
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

    explicit cvk_exception(cth::except::default_exception defaultException)
        : default_exception{std::move(defaultException)} { prepend(PREFIX); }
};

/**
 * exception of a vulkan call
 * @details holds the VkResult
 */
class vk_result_exception : public cvk_exception {
    static constexpr auto SEVERITY = cth::except::ERR;
    static constexpr std::string_view MESSAGE_TEMPLATE = "vk error: {}";

public:
    vk_result_exception(
        std::string_view msg,
        cth::except::Severity severity = SEVERITY,
        std::source_location sourceLocation = {},
        std::stacktrace stacktrace = {}
    ) : cvk_exception{
        std::format(MESSAGE_TEMPLATE, msg),
        severity,
        std::move(sourceLocation),
        std::move(stacktrace)
    } {}

    vk_result_exception(
        VkResult result,
        std::string_view msg,
        std::source_location sourceLocation = {},
        std::stacktrace stacktrace = {}
    ) : vk_result_exception{msg, SEVERITY, std::move(sourceLocation), std::move(stacktrace)} { add(result); }

    vk_result_exception(VkResult result, cth::except::default_exception defaultException)
        : cvk_exception{std::move(defaultException)} { add(result); }

    using cvk_exception::add;

    /**
     * stores the result and adds it to the message
     */
    vk_result_exception& add(VkResult result) {
        _vkResult = result;
        return cvk_exception::add("VkResult: ({})", result);
    }

    [[nodiscard]] VkResult vkResult() const noexcept { return _vkResult; }

private:
    VkResult _vkResult = VK_RESULT_MAX_ENUM;
};
}

#define CVK_STABLE_THROW(expression, msg, ...) \
    CTH_STABLE_THROW_T(cvk::cvk_exception, expression, msg, __VA_ARGS__)

#define CVK_STABLE_ERR(expression, msg, ...) \
    CTH_STABLE_ERR_T(cvk::cvk_exception, expression, msg, __VA_ARGS__)

#define CVK_STABLE_WARN(expression, msg, ...) \
    CTH_STABLE_WARN_T(cvk::cvk_exception, expression, msg, __VA_ARGS__)

#define CVK_VK_STABLE_THROW(expression, msg, ...) \
    CTH_STABLE_THROW_T(cvk::vk_result_exception, expression, msg, __VA_ARGS__)

/**
 * acts like @ref CVK_VK_STABLE_THROW and adds the result to the exception
 * @param expression true -> throws
 * @param result to add to the exception
 * @param msg format string
 * @details braces and else work like the other CTH_ log macros
 */
#define CVK_RESULT_STABLE_THROW(expression, result, msg, ...)\
    CVK_VK_STABLE_THROW(expression, msg, __VA_ARGS__) \
        for(auto&& _ : {(details->e().add(result), 0)})
