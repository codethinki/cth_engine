// ReSharper disable CppClangTidyCertDcl58Cpp
#pragma once
#include "format/cth_vk_format_string.hpp"
#include "format/cth_vk_to_string.hpp"

#include <boost/pfr.hpp>
#include <vulkan/vulkan.h>

namespace cth::vk::fmt {
template<class T>
concept formattable_enum = cth::type::is_any_of<type::pure_t<T>,
    VkResult, VkFormat,
    VkStructureType, VkDescriptorType,
    VkColorSpaceKHR, VkPresentModeKHR
>;

template<class T>
concept formattable_type = cth::type::is_any_of<type::pure_t<T>,
    VkSurfaceFormatKHR
>;


}

//formattable_enum
template<class T> requires (cth::vk::fmt::formattable_enum<T>)
struct std::formatter<T> : std::formatter<int> {
    constexpr auto parse(format_parse_context& ctx) { return std::formatter<int>::parse(ctx); }

    template<typename FormatContext>
    auto format(T const& obj, FormatContext& ctx) const { return std::format_to(ctx.out(), "{{{}}}", cth::vk::fmt::to_string(obj)); }
};

//formattable_type
template<class T> requires (cth::vk::fmt::formattable_type<T>)
struct std::formatter<T> : std::formatter<int> {
    static constexpr auto fmt_base = cth::vk::fmt::format_string<T>();

    constexpr auto parse(format_parse_context& ctx) { return std::formatter<int>::parse(ctx); }

    template<typename FormatContext>
    auto format(T const& obj, FormatContext& ctx) const {
        auto tuple = boost::pfr::structure_to_tuple(obj);
        return std::apply([&ctx]<typename... U>(U&&... args) { return std::format_to(ctx.out(), std::string_view{fmt_base}, std::forward<U>(args)...); },
            tuple);
    }
};
