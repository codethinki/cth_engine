#pragma once
#include "format.hpp"

#include <volk.h>
#include <cth/exception.hpp>



namespace jvk {
class result_exception final : public cth::except::default_exception {
public:
    result_exception(VkResult result, default_exception ex) : default_exception(ex), _vkResult(result) {
        ex.add("VkResult: ({0})", result);
    }

    [[nodiscard]] VkResult result() const noexcept { return _vkResult; }

private:
    VkResult _vkResult;
};
}
