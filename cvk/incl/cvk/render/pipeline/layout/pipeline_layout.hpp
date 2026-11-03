#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
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
class DescriptorSetLayout;

/**
 * VkPipelineLayout wrapper
 */
class PipelineLayout {
public:
    struct Config;
    struct State;

    /**
     * base constructor, copies the handles of the set layouts
     * @pre @ref Core::created()
     * @pre the set layouts are created
     */
    PipelineLayout(Core const&, Config const&);

    /**
     * constructs and wraps
     * @details calls:
     * - @ref PipelineLayout(Core const&, Config const&)
     * - @ref wrap()
     */
    PipelineLayout(Core const&, Config const&, State const&);

    /**
     * constructs and creates
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
     * wraps the state
     * @pre the layout was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * creates the layout
     * @pre the set layouts of the config are not destroyed
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: more set layouts than maxBoundDescriptorSets
     * @throws cvk::vk_result_exception result of vkCreatePipelineLayout()
     */
    void create();

    /**
     * destroys and forgets
     * @pre @ref created()
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr VkPipelineLayout get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

    /**
     * @return handle of the set layout per set number
     */
    [[nodiscard]] constexpr std::span<VkDescriptorSetLayout const> setLayouts() const { return _vkSetLayouts; }

private:
    void forget();

    core_handle<VkPipelineLayout> _handle;
    std::vector<VkDescriptorSetLayout> _vkSetLayouts;
    std::vector<VkPushConstantRange> _pushConstantRanges;

public:
    PipelineLayout(PipelineLayout const& other) = delete;
    PipelineLayout(PipelineLayout&& other) noexcept = default;
    PipelineLayout& operator=(PipelineLayout const& other) = delete;
    PipelineLayout& operator=(PipelineLayout&& other) noexcept = default;

    static constexpr void debug_check(PipelineLayout const&);
};
}

namespace cvk {
struct PipelineLayout::Config {
    /**
     * set layout per set number, index == set number
     */
    std::vector<not_null_ptr<DescriptorSetLayout const>> setLayouts{};

    std::vector<VkPushConstantRange> pushConstantRanges{};
};

struct PipelineLayout::State {
    vk_not_null<VkPipelineLayout> vkLayout;

    /**
     * the raw handles to destroy, in destruction order
     */
    [[nodiscard]] constexpr std::tuple<VkPipelineLayout> decompose() && { return {vkLayout.get()}; }
};
}

namespace cvk {
constexpr void PipelineLayout::debug_check(PipelineLayout const& layout) {
    CTH_CRITICAL(!layout.created(), "pipeline layout must be created") {}
}
}
