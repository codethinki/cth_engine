// ReSharper disable CppClangTidyCertDcl58Cpp
#pragma once
#include "format/cth_vk_format_string.hpp"
#include "format/cth_vk_to_string.hpp"


#include <volk.h>
#include <cth/string/format.hpp>


namespace cth::vk::fmt {
template<class T>
std::string structure_to_string(T&& to_tuple) {
    constexpr static auto FMT_BASE = cth::vk::fmt::format_string<std::remove_cv_t<T>>();

    auto tuple = boost::pfr::structure_to_tuple(std::forward<T>(to_tuple));

    return std::apply(
        []<typename... U>(U&&... args) { return std::format(std::string_view{FMT_BASE}, std::forward<U>(args)...); },
        tuple
    );
}

template<class T>
concept formattable_type = cth::type::is_any_of<type::pure_t<T>,
    VkSurfaceFormatKHR
>;


}


CTH_FORMAT_TYPE(VkResult, cth::vk::fmt::to_string);
CTH_FORMAT_TYPE(VkFormat, cth::vk::fmt::to_string);
CTH_FORMAT_TYPE(VkStructureType, cth::vk::fmt::to_string);
CTH_FORMAT_TYPE(VkDescriptorType, cth::vk::fmt::to_string);
CTH_FORMAT_TYPE(VkColorSpaceKHR, cth::vk::fmt::to_string);
CTH_FORMAT_TYPE(VkPresentModeKHR, cth::vk::fmt::to_string);
CTH_FORMAT_TYPE(VkObjectType, cth::vk::fmt::to_string);

//formattable_type
template<class T> requires (cth::vk::fmt::formattable_type<T>)
struct std::formatter<T> : std::formatter<int> {
    static constexpr auto fmt_base = cth::vk::fmt::format_string<T>();

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
