#include "cvk/render/pipeline/layout/pipeline_layout.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <ranges>


namespace cvk {
PipelineLayout::PipelineLayout(Core const& core, Config const& config) : _handle{core},
    _pushConstantRanges{config.pushConstantRanges} {
    Core::debug_check(core);
    auto const maxSets = core.physicalDevice().limits().maxBoundDescriptorSets;
    CTH_STABLE_ERR(config.setLayouts.size() > maxSets, "more set layouts than maxBoundDescriptorSets") {
        details->add("set layouts: {}, max: {}", config.setLayouts.size(), maxSets);
        throw details->exception();
    }

    _vkSetLayouts.reserve(config.setLayouts.size());
    for(auto const& layout : config.setLayouts) {
        DescriptorSetLayout::debug_check(*layout);
        _vkSetLayouts.push_back(layout->get());
    }
}

PipelineLayout::PipelineLayout(Core const& core, Config const& config, State const& state) : PipelineLayout{
    core,
    config
} { wrap(state); }

PipelineLayout::PipelineLayout(Core const& core, Config const& config, create_t) : PipelineLayout{core, config} {
    create();
}

void PipelineLayout::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkLayout.get(), core());
}

void PipelineLayout::create() {
    optDestroy();

    VkPipelineLayoutCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(_vkSetLayouts.size()),
        .pSetLayouts = _vkSetLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size()),
        .pPushConstantRanges = _pushConstantRanges.data(),
    };

    VkPipelineLayout ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreatePipelineLayout(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create pipeline layout") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void PipelineLayout::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

PipelineLayout::State PipelineLayout::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void PipelineLayout::destroy(DeviceTable table, VkPipelineLayout vkLayout) {
    CTH_WARN(vkLayout == VK_NULL_HANDLE, "vkLayout invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipelineLayout(table.device(), vkLayout, nullptr);
}

void PipelineLayout::forget() {}
}
