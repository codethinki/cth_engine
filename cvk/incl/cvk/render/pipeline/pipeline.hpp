#pragma once
#include "pipeline_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/utility.hpp>



namespace cvk {
class Core;
class PipelineLayout;

/**
 * @brief graphics VkPipeline wrapper for dynamic rendering
 */
class Pipeline {
public:
    using Config = PipelineConfig;
    struct State;

    /**
     * @brief base constructor, copies the handle of the layout
     * @pre @ref Core::created()
     * @pre @ref PipelineLayout::created()
     */
    Pipeline(Core const&, PipelineLayout const&, Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Pipeline(Core const&, PipelineLayout const&, Config)
     * - @ref wrap()
     */
    Pipeline(Core const&, PipelineLayout const&, Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Pipeline(Core const&, PipelineLayout const&, Config)
     * - @ref create()
     */
    explicit Pipeline(Core const&, PipelineLayout const&, Config, create_t);

    /**
     * @brief base constructor of a derivative pipeline, copies the handle and the layout of the parent
     * @pre @ref Core::created()
     * @pre @ref created() of the parent, with VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT in its flags
     */
    Pipeline(Core const&, Pipeline const& parent, Config);

    /**
     * @brief constructs and creates a derivative pipeline
     * @details calls:
     * - @ref Pipeline(Core const&, Pipeline const&, Config)
     * - @ref create()
     */
    explicit Pipeline(Core const&, Pipeline const& parent, Config, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~Pipeline() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the pipeline was created with the config and the layout
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the pipeline
     * @details derivative pipeline -> adds VK_PIPELINE_CREATE_DERIVATIVE_BIT
     * @pre the layout, the parent and the shader modules of the config are not destroyed
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateGraphicsPipelines()
     */
    void create();

    /**
     * @brief destroys and resets
     * @pre @ref created()
     * @pre the gpu does not use the pipeline
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and resets
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkPipeline);

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkPipeline get() const { return _handle.get(); }
    [[nodiscard]] VkPipelineLayout layout() const { return _vkLayout.get(); }

    /**
     * @return VK_NULL_HANDLE -> not a derivative pipeline
     */
    [[nodiscard]] VkPipeline parent() const { return _vkParent; }
    [[nodiscard]] Config const& config() const { return _config; }
    [[nodiscard]] DeviceTable deviceTable() const { return _deviceTable; }

private:
    void reset();

    DeviceTable _deviceTable;
    vk_not_null<VkPipelineLayout> _vkLayout;
    VkPipeline _vkParent = VK_NULL_HANDLE;
    Config _config;
    move_ptr<VkPipeline_T> _handle = VK_NULL_HANDLE;

public:
    Pipeline(Pipeline const& other) = delete;
    Pipeline(Pipeline&& other) noexcept = default;
    Pipeline& operator=(Pipeline const& other) = delete;
    Pipeline& operator=(Pipeline&& other) noexcept { return cth::reconstruct_at(*this, std::move(other)); }

    static void debug_check(Pipeline const&);
};
}

namespace cvk {
struct Pipeline::State {
    vk_not_null<VkPipeline> vkPipeline;
};
}

namespace cvk {
inline void Pipeline::debug_check(Pipeline const& pipeline) {
    CTH_CRITICAL(!pipeline.created(), "pipeline must be created") {}
}
}
