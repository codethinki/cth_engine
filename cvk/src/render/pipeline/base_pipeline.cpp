#include "cvk/render/pipeline/base_pipeline.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
BasePipeline::BasePipeline(Core const& core, VkPipelineBindPoint bindPoint) : _handle{core},
    _bindPoint{bindPoint} {}

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

void destroy(DeviceTable table, VkPipeline vkPipeline) {
    CTH_WARN(vkPipeline == VK_NULL_HANDLE, "vkPipeline invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipeline(table.device(), vkPipeline, nullptr);
}

void BasePipeline::create(VkGraphicsPipelineCreateInfo const& info) {
    Core::debug_check(core());

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
    Core::debug_check(core());

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

BasePipeline::MappedStages BasePipeline::mapStages(
    std::span<VkPipelineShaderStageCreateInfo const> stages,
    std::span<VkDescriptorSetAndBindingMappingEXT const> mappings
) {
    MappedStages mapped{.mappingInfos = {}, .stages = {std::from_range, stages}};
    if(mappings.empty())
        return mapped;

    mapped.mappingInfos.reserve(stages.size());
    for(auto& stage : mapped.stages) {
        auto const& mappingInfo = mapped.mappingInfos.emplace_back(
            VkShaderDescriptorSetAndBindingMappingInfoEXT{
                .sType = VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT,
                .pNext = stage.pNext,
                .mappingCount = static_cast<uint32_t>(mappings.size()),
                .pMappings = mappings.data(),
            }
        );
        stage.pNext = &mappingInfo;
    }
    return mapped;
}

void BasePipeline::forget() {}
}
