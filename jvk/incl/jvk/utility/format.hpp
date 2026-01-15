// ReSharper disable CppClangTidyCertDcl58Cpp
#pragma once
#include "format/format_string.hpp"
#include "format/vk_to_string.hpp"


#include <volk.h>
#include <cth/string/format.hpp>
#include <cth/meta/utility.hpp>
#include <cth/meta/variadic.hpp>

namespace jvk::fmt {
template<class T>
std::string structure_to_string(T&& to_tuple) {
    constexpr static auto FMT_BASE = jvk::fmt::format_string<std::remove_cv_t<T>>();

    auto tuple = boost::pfr::structure_to_tuple(std::forward<T>(to_tuple));

    return std::apply(
        []<typename... U>(U&&... args) { return std::format(std::string_view{FMT_BASE}, std::forward<U>(args)...); },
        tuple
    );
}

template<class T>
concept formattable_type = cth::mta::any_of<cth::mta::pure_t<T>,
    VkSurfaceFormatKHR,
    VkExtent2D
>;


}


CTH_FORMAT_TYPE(VkResult, jvk::fmt::to_string);

CTH_FORMAT_TYPE(VkFormat, jvk::fmt::to_string);

CTH_FORMAT_TYPE(VkStructureType, jvk::fmt::to_string);

CTH_FORMAT_TYPE(VkDescriptorType, jvk::fmt::to_string);

CTH_FORMAT_TYPE(VkColorSpaceKHR, jvk::fmt::to_string);

CTH_FORMAT_TYPE(VkPresentModeKHR, jvk::fmt::to_string);

CTH_FORMAT_TYPE(VkObjectType, jvk::fmt::to_string);

//formattable_type
template<class T> requires (jvk::fmt::formattable_type<T>)
struct std::formatter<T> : std::formatter<int> {
    static constexpr auto fmt_base = jvk::fmt::format_string<T>();

    constexpr auto parse(format_parse_context& ctx) { return std::formatter<int>::parse(ctx); }

    template<typename FormatContext>
    auto format(T const& obj, FormatContext& ctx) const {
        auto tuple = boost::pfr::structure_to_tuple(obj);
        return std::apply([&ctx]<typename... U>(U&&... args) {
                return std::format_to(ctx.out(), std::string_view{fmt_base}, std::forward<U>(args)...);
            },
            tuple);
    }
};
