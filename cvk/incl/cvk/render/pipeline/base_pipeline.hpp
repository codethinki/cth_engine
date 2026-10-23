#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>


namespace cvk {
class Core;
class PipelineLayout;

/**
 * @brief VkPipeline wrapper, base of @ref GraphicsPipeline and @ref ComputePipeline
 * @details the derived classes build the create info, the base creates the pipeline,
 *  the layout is a parent of the pipeline: it outlives the pipeline,
 *  the destructor is protected: delete through the derived class
 */
class BasePipeline {
public:
    using vk_handle_t = VkPipeline;
    struct State;

    /**
     * @brief wraps the state
     * @pre the pipeline was created with the layout and the config of the derived class
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief destroys and forgets
     * @pre @ref created()
     * @pre the gpu does not use the pipeline
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkPipeline);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr PipelineLayout const& layout() const { return *_layout; }
    [[nodiscard]] constexpr VkPipelineBindPoint bindPoint() const { return _bindPoint; }

protected:
    /**
     * @brief base constructor, keeps the layout, the core comes from the layout
     * @pre @ref Core::created() of the layout
     */
    BasePipeline(PipelineLayout const&, VkPipelineBindPoint);

    /**
     * @note calls @ref optDestroy()
     */
    ~BasePipeline() { optDestroy(); }

    /**
     * @brief creates the pipeline with the info
     * @pre the layout of the info is @ref layout()
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateGraphicsPipelines()
     */
    void create(VkGraphicsPipelineCreateInfo const&);

    /**
     * @brief creates the pipeline with the info
     * @pre the layout of the info is @ref layout()
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateComputePipelines()
     */
    void create(VkComputePipelineCreateInfo const&);

private:
    void forget();

    core_handle<BasePipeline> _handle;
    not_null_ptr<PipelineLayout const> _layout;
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
};
}

namespace cvk {
constexpr void BasePipeline::debug_check(BasePipeline const& pipeline) {
    CTH_CRITICAL(!pipeline.created(), "pipeline must be created") {}
}
}
