#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <span>
#include <type_traits>
#include <vector>


namespace cvk {
class BaseCmdBuffer;
class Core;
class DescriptorSetLayout;

/**
 * @brief VkPipelineLayout wrapper
 */
class PipelineLayout {
public:
    using vk_handle_t = VkPipelineLayout;
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
     * @brief destroys and forgets
     * @pre @ref created()
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

    /**
     * @brief records push constants
     * @param offset in bytes
     * @pre @ref created(), @ref BaseCmdBuffer::recording()
     * @pre 0 < data.size(), offset and data.size() are multiples of 4
     * @pre the push constant ranges of the config cover the bytes for the stages
     */
    void pushConstants(
        BaseCmdBuffer const&,
        VkShaderStageFlags stages,
        std::span<char const> data,
        uint32_t offset = 0
    ) const;

    /**
     * @brief records the value as push constants
     * @details calls @ref pushConstants(BaseCmdBuffer const&, VkShaderStageFlags, std::span<char const>, uint32_t)
     */
    template<class T> requires std::is_trivially_copyable_v<T>
    void pushConstants(
        BaseCmdBuffer const& cmdBuffer,
        VkShaderStageFlags stages,
        T const& value,
        uint32_t offset = 0
    ) const {
        pushConstants(cmdBuffer, stages, std::span{reinterpret_cast<char const*>(&value), sizeof(T)}, offset);
    }

    static void destroy(DeviceTable table, VkPipelineLayout);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

private:
    void forget();

    core_handle<PipelineLayout> _handle;
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
constexpr void PipelineLayout::debug_check(PipelineLayout const& layout) {
    CTH_CRITICAL(!layout.created(), "pipeline layout must be created") {}
}
}
