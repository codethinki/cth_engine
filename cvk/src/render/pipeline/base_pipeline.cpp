#include "cvk/render/pipeline/base_pipeline.hpp"

#include "cvk/base/core.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
BasePipeline::BasePipeline(Core const& core, PipelineLayout const& layout, VkPipelineBindPoint bindPoint) :
    _handle{core}, _layout{&layout}, _bindPoint{bindPoint} {
    Core::debug_check(core);
}

void BasePipeline::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkPipeline.get(), core());
}

void BasePipeline::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

BasePipeline::State BasePipeline::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void BasePipeline::destroy(DeviceTable table, VkPipeline vkPipeline) {
    CTH_WARN(vkPipeline == VK_NULL_HANDLE, "vkPipeline invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipeline(table.device(), vkPipeline, nullptr);
}

void BasePipeline::create(VkGraphicsPipelineCreateInfo const& info) {
    PipelineLayout::debug_check(*_layout);
    CTH_CRITICAL(info.layout != _layout->get(), "the create info must use the layout of the pipeline") {}

    optDestroy();

    VkPipeline ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateGraphicsPipelines(
        core().vkDevice(),
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &ptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create graphics pipeline") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void BasePipeline::create(VkComputePipelineCreateInfo const& info) {
    PipelineLayout::debug_check(*_layout);
    CTH_CRITICAL(info.layout != _layout->get(), "the create info must use the layout of the pipeline") {}

    optDestroy();

    VkPipeline ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateComputePipelines(
        core().vkDevice(),
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &ptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create compute pipeline") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void BasePipeline::forget() {}
}
