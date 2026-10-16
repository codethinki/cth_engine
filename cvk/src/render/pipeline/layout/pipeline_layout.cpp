#include "cvk/render/pipeline/layout/pipeline_layout.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <ranges>


namespace cvk {
PipelineLayout::PipelineLayout(Core const& core, Config const& config) : _deviceTable{core.deviceTable()},
    _pushConstantRanges{config.pushConstantRanges} {
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

    _handle = state.vkLayout.get();
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
    auto const result = _deviceTable->vkCreatePipelineLayout(_deviceTable.device(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create pipeline layout") {
        reset();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle = ptr;
}

void PipelineLayout::destroy() {
    debug_check(*this);

    destroy(_deviceTable, _handle.get());

    reset();
}

PipelineLayout::State PipelineLayout::release() {
    debug_check(*this);

    State const state{_handle.get()};
    reset();
    return state;
}

void PipelineLayout::destroy(DeviceTable table, VkPipelineLayout vkLayout) {
    CTH_WARN(vkLayout == VK_NULL_HANDLE, "vkLayout invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipelineLayout(table.device(), vkLayout, nullptr);
}

void PipelineLayout::reset() { _handle = VK_NULL_HANDLE; }
}
