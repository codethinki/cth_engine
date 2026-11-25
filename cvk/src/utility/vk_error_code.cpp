#include "cvk/utility/vk_error_code.hpp"

#include <vulkan/vk_enum_string_helper.h>

#include <string>


namespace cvk {
namespace {
    class vk_error_category final : public std::error_category {
    public:
        [[nodiscard]] char const* name() const noexcept override { return "vulkan"; }

        [[nodiscard]] std::string message(int value) const override {
            return string_VkResult(static_cast<VkResult>(value));
        }
    };
}

std::error_category const& vk_category() noexcept {
    static vk_error_category const category{};
    return category;
}

std::error_code make_error_code(VkResult result) noexcept {
    return std::error_code{static_cast<int>(result), vk_category()};
}
}
