#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <vector>


namespace cvk {
class Core;
class DescriptorSetLayout;

/**
 * @brief VkPipelineLayout wrapper
 */
class PipelineLayout {
public:
    struct Config;
    struct State;

    /**
     * @brief base constructor, copies the handles of the set layouts
     * @pre @ref Core::created()
     * @pre the set layouts are created
     * @throws cth::except::default_exception reason: more set layouts than maxBoundDescriptorSets
     */
    PipelineLayout(Core const&, Config const&);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref PipelineLayout(Core const&, Config const&)
     * - @ref wrap()
     */
    PipelineLayout(Core const&, Config const&, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref PipelineLayout(Core const&, Config const&)
     * - @ref create()
     */
    explicit PipelineLayout(Core const&, Config const&, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~PipelineLayout() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the layout was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the layout
     * @pre the set layouts of the config are not destroyed
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreatePipelineLayout()
     */
    void create();

    /**
     * @brief destroys and invalidates
     * @pre @ref created()
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and invalidates
     * @pre @ref created()
     */
    State release();

    static void destroy(DeviceTable table, VkPipelineLayout);

    [[nodiscard]] bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] VkPipelineLayout get() const { return _handle.get(); }
    [[nodiscard]] Core const& core() const { return _handle.core(); }

private:
    void invalidate();

    core_handle<VkPipelineLayout, PipelineLayout> _handle;
    std::vector<VkDescriptorSetLayout> _vkSetLayouts;
    std::vector<VkPushConstantRange> _pushConstantRanges;

public:
    PipelineLayout(PipelineLayout const& other) = delete;
    PipelineLayout(PipelineLayout&& other) noexcept = default;
    PipelineLayout& operator=(PipelineLayout const& other) = delete;
    PipelineLayout& operator=(PipelineLayout&& other) noexcept = default;

    static void debug_check(PipelineLayout const&);
};
}

namespace cvk {
struct PipelineLayout::Config {
    /**
     * @brief set layout per set number, index == set number
     */
    std::vector<not_null<DescriptorSetLayout const*>> setLayouts{};

    std::vector<VkPushConstantRange> pushConstantRanges{};
};

struct PipelineLayout::State {
    vk_not_null<VkPipelineLayout> vkLayout;
};
}

namespace cvk {
inline void PipelineLayout::debug_check(PipelineLayout const& layout) {
    CTH_CRITICAL(!layout.created(), "pipeline layout must be created") {}
}
}
