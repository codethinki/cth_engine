#pragma once
#include <volk.h>

#include <system_error>


namespace cvk {
// own header next to vk_exceptions.hpp: error codes are no exceptions
// no std::is_error_code_enum<VkResult>: VkResult lives in the global namespace, the implicit
// conversion finds make_error_code() only through ADL there, a global make_error_code() would be needed
/**
 * error category of `VkResult`
 * @details
 * - name `"vulkan"`
 * - `message()`: `string_VkResult()` of the value
 */
[[nodiscard]] std::error_category const& vk_category() noexcept;

/**
 * @return error code of the result in `vk_category()`
 * @details `VK_SUCCESS` -> empty code, every other result -> set code, also the positive ones like `VK_TIMEOUT`
 */
[[nodiscard]] std::error_code make_error_code(VkResult result) noexcept;
}
