#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
DescriptorSetLayout::DescriptorSetLayout(Core const& core, Config config) : _handle{core},
    _bindings{std::move(config.bindings)} { Core::debug_check(core); }

DescriptorSetLayout::DescriptorSetLayout(Core const& core, Config config, State const& state) : DescriptorSetLayout{
    core,
    std::move(config)
} { wrap(state); }

DescriptorSetLayout::DescriptorSetLayout(Core const& core, Config config, create_t) : DescriptorSetLayout{
    core,
    std::move(config)
} { create(); }

void DescriptorSetLayout::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkLayout.get(), core());
}

void DescriptorSetLayout::create() {
    optDestroy();

    VkDescriptorSetLayoutCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(_bindings.size()),
        .pBindings = _bindings.data(),
    };

    VkDescriptorSetLayout ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateDescriptorSetLayout(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create descriptor set layout") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void DescriptorSetLayout::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

DescriptorSetLayout::State DescriptorSetLayout::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void DescriptorSetLayout::destroy(DeviceTable table, VkDescriptorSetLayout vkLayout) {
    CTH_WARN(vkLayout == VK_NULL_HANDLE, "vkLayout invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyDescriptorSetLayout(table.device(), vkLayout, nullptr);
}

std::span<VkDescriptorSetLayoutBinding const> DescriptorSetLayout::bindings() const { return _bindings; }

VkDescriptorSetLayoutBinding const* DescriptorSetLayout::binding(uint32_t binding) const {
    auto const it = std::ranges::lower_bound(_bindings, binding, {}, &VkDescriptorSetLayoutBinding::binding);
    return it != _bindings.end() && it->binding == binding ? std::to_address(it) : nullptr;
}

void DescriptorSetLayout::forget() {}
}
