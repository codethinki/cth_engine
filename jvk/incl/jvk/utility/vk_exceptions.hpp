#pragma once
#include "format.hpp"

#include <volk.h>
#include <cth/exception.hpp>
#include <cth/io/log.hpp>


namespace jvk {

class jvk_exception : public cth::except::default_exception {
    static cxpr auto PREFIX = "jvk: ";

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

    jvk_exception(cth::except::default_exception default_exception)
        : default_exception{std::move(default_exception)} { prepend(PREFIX); }
};

class vk_result_exception : public jvk_exception {
    static cxpr auto SEVERITY = cth::except::ERR;
    static cxpr auto PREFIX = "jvk: ";
    static cxpr std::string_view MESSAGE_TEMPLATE = "vk error: {}";

public:
    vk_result_exception(
        std::string_view msg,
        cth::except::Severity severity = SEVERITY,
        std::source_location source_location = {},
        std::stacktrace stacktrace = {}
    ) : jvk_exception{
        std::format(MESSAGE_TEMPLATE, msg),
        severity,
        std::move(source_location),
        std::move(stacktrace)
    } {}

    vk_result_exception(
        VkResult result,
        std::string_view msg,
        std::source_location source_location = {},
        std::stacktrace stacktrace = {}
    ) : vk_result_exception{msg, SEVERITY, std::move(source_location), std::move(stacktrace)} { add(result); }

    vk_result_exception(VkResult result, cth::except::default_exception default_exception) : jvk_exception{
        std::move(default_exception)
    } {
        prepend(PREFIX);
        add(result);
    }


    declauto add(VkResult result) {
        _vkResult = result;
        return jvk_exception::add("VkResult: ({})", result);
    }

private:
    VkResult _vkResult = VK_RESULT_MAX_ENUM;

public:
    [[nodiscard]] VkResult vkResult() const noexcept { return _vkResult; }
};

}

#define JVK_STABLE_THROW(expression, msg, ...) \
    CTH_STABLE_THROW_T(jvk::jvk_exception, expression, msg, __VA_ARGS__)

#define JVK_VK_STABLE_THROW(expression, msg, ...) \
    CTH_STABLE_THROW_T(jvk::vk_result_exception, expression, msg, __VA_ARGS__)

/**
 * acts like @ref JVK_VK_STABLE_THROW but has a dedicated result handling mechanism
 * @param expression throws on true
 * @param result to add to exception
 * @param msg format string
 * @details braces and else are consistent with previous log framework. 
 *  uses a range for loop over a one element init list. the element is computed
 *  via paren operator to allow the side effect execution first
 */
#define JVK_RESULT_STABLE_THROW(expression, result, msg, ...)\
    JVK_VK_STABLE_THROW(expression, msg, __VA_ARGS__) \
        for(auto&& _ : {(details->e().add(result), 0)})
