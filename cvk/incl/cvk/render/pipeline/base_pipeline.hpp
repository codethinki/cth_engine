#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <tuple>


namespace cvk {
class Core;
class PipelineLayout;

/**
 * `VkPipeline` wrapper
 * @details base of `GraphicsPipeline` and `ComputePipeline`
 * @details the layout outlives the pipeline
 */
class BasePipeline {
public:
    struct State;

    /**
     * wraps the state
     * @pre the pipeline was created with the layout and the config of the derived class
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
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre `created()`
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkPipeline get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr PipelineLayout const& layout() const { return *_layoutPtr; }
    [[nodiscard]] constexpr VkPipelineBindPoint bindPoint() const { return _bindPoint; }

protected:
    /**
     * base constructor, keeps the layout
     * @pre `Core::created()` of the layout
     */
    BasePipeline(PipelineLayout const&, VkPipelineBindPoint);

    /**
     * @note calls `optDestroy()`
     */
    ~BasePipeline() { optDestroy(); }

    /**
     * creates the pipeline with the info
     * @pre the layout of the info is `layout()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateGraphicsPipelines()`
     */
    void create(VkGraphicsPipelineCreateInfo const&);

    /**
     * creates the pipeline with the info
     * @pre the layout of the info is `layout()`
     * @post `created()`
     * @note calls `optDestroy()`
     * @throws cvk::vk_result_exception result of `vkCreateComputePipelines()`
     */
    void create(VkComputePipelineCreateInfo const&);

private:
    void forget();

    core_handle<VkPipeline> _handle;
    not_null_ptr<PipelineLayout const> _layoutPtr;
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
