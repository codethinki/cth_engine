#pragma once
#include "cth_vk_format.hpp"

#include<cth/exception.hpp>
#include <vulkan/vulkan.h>



namespace cth::vk {
class result_exception final : public except::default_exception {
public:
    result_exception(VkResult result, default_exception ex) : default_exception(ex), _vkResult(result) { ex.add("VkResult: ({0})", result); }
    [[nodiscard]] VkResult result() const noexcept { return _vkResult; }

private:
    VkResult _vkResult;
};
}
