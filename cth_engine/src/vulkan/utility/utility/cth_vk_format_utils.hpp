#pragma once
template<>
struct std::formatter<VkStructureType> : std::formatter<int> {
    constexpr auto parse(format_parse_context& ctx) {
        return std::formatter<int>::parse(ctx); // Often sufficient
    }

    template<typename FormatContext>
    auto format(VkStructureType const& obj, FormatContext& ctx) const { return format_to(ctx.out(), "MyCustomType(value={})", static_cast<uint32_t>(obj)); }
};
