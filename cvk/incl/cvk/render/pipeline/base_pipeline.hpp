#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <span>
#include <tuple>
#include <vector>


namespace cvk {
class Core;

/**
 * `VkPipeline` wrapper
 * @details base of `GraphicsPipeline` and `ComputePipeline`
 * @details created with `VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT`: the shaders read the descriptors from the
 *  bound heaps, see `ResourceHeap`, `SamplerHeap`
 */
class BasePipeline {
public:
    struct State;

    /**
     * wraps the state
     * @pre the pipeline was created with the config of the derived class
     * @post `created()`
     * @note calls `optDestroy()`
     */
    void wrap(State const&);

    /**
     * destroys and forgets
     * @pre `created()`
     * @pre the gpu does not use the pipeline
     */
    void destroy();

    /**
     * if `created()` -> calls `destroy()`
     */
    void optDestroy() {
        if(created())
            destroy();
    }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkPipeline get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr VkPipelineBindPoint bindPoint() const { return _bindPoint; }

protected:
    /**
     * stages with the descriptor mappings in front of their `pNext` chains
     * @attention the stages point into `mappingInfos`, the infos into the mappings
     */
    struct MappedStages {
        std::vector<VkShaderDescriptorSetAndBindingMappingInfoEXT> mappingInfos;
        std::vector<VkPipelineShaderStageCreateInfo> stages;
    };

    /**
     * base constructor
     */
    BasePipeline(Core const&, VkPipelineBindPoint);

    /**
     * @note calls `optDestroy()`
     */
    ~BasePipeline() { optDestroy(); }

    /**
     * creates the pipeline with the info
     * @pre the `pNext` chain of the info has `VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateGraphicsPipelines()`
     */
    void create(VkGraphicsPipelineCreateInfo const&);

    /**
     * creates the pipeline with the info
     * @pre the `pNext` chain of the info has `VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateComputePipelines()`
     */
    void create(VkComputePipelineCreateInfo const&);

    /**
     * @return the stages with the mappings, empty mappings -> the stages unchanged
     */
    [[nodiscard]] static MappedStages mapStages(
        std::span<VkPipelineShaderStageCreateInfo const> stages,
        std::span<VkDescriptorSetAndBindingMappingEXT const> mappings
    );

    /**
     * @return the create flags with `VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT` in front of `next`
     */
    [[nodiscard]] static constexpr VkPipelineCreateFlags2CreateInfo createFlags(
        VkPipelineCreateFlags2 flags,
        void const* next
    ) {
        return VkPipelineCreateFlags2CreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
            .pNext = next,
            .flags = flags | VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
        };
    }

private:
    void forget();

    core_handle<VkPipeline> _handle;
    VkPipelineBindPoint _bindPoint;

public:
    BasePipeline(BasePipeline const& other) = delete;
    BasePipeline(BasePipeline&& other) noexcept = default;
    BasePipeline& operator=(BasePipeline const& other) = delete;
    BasePipeline& operator=(BasePipeline&& other) noexcept = default;

    static constexpr void debug_check(BasePipeline const&);
};
}

namespace cvk {
struct BasePipeline::State {
    vk_not_null<VkPipeline> vkPipeline;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkPipeline> decompose() && { return {vkPipeline.get()}; }
};
}

namespace cvk {
constexpr void BasePipeline::debug_check(BasePipeline const& pipeline) {
    CTH_CRITICAL(!pipeline.created(), "pipeline must be created") {}
}
}
