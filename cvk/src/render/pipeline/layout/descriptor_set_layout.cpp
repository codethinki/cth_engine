#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
DescriptorSetLayout::DescriptorSetLayout(Core const& core, Config config) : _handle{core} {
    Core::debug_check(core);
    Config::debug_check(config);

    _bindings = std::move(config.bindings);
    _bindingFlags = std::move(config.bindingFlags);
}

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

    VkDescriptorSetLayoutBindingFlagsCreateInfo const flagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<uint32_t>(_bindingFlags.size()),
        .pBindingFlags = _bindingFlags.data(),
    };
    VkDescriptorSetLayoutCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = _bindingFlags.empty() ? nullptr : &flagsInfo,
        .flags = updateAfterBind() ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0u,
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

void DescriptorSetLayout::forget() {}
}
