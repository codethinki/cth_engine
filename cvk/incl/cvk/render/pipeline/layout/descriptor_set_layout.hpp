#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>


namespace cvk {
class Core;

/**
 * @brief VkDescriptorSetLayout wrapper
 */
class DescriptorSetLayout {
public:
    using vk_handle_t = VkDescriptorSetLayout;
    struct Config;
    struct State;

    /**
     * @brief base constructor
     * @pre @ref Core::created()
     */
    DescriptorSetLayout(Core const&, Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref DescriptorSetLayout(Core const&, Config)
     * - @ref wrap()
     */
    DescriptorSetLayout(Core const&, Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref DescriptorSetLayout(Core const&, Config)
     * - @ref create()
     */
    explicit DescriptorSetLayout(Core const&, Config, create_t);

    /**
     * @note calls @ref optDestroy()
     */
    ~DescriptorSetLayout() { optDestroy(); }

    /**
     * @brief wraps the state
     * @pre the layout was created with the config
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief creates the layout
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateDescriptorSetLayout()
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

    static void destroy(DeviceTable table, VkDescriptorSetLayout);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }

    /**
     * @return bindings, sorted by binding number
     */
    [[nodiscard]] std::span<VkDescriptorSetLayoutBinding const> bindings() const;

    /**
     * @return nullptr -> no binding with that number
     */
    [[nodiscard]] VkDescriptorSetLayoutBinding const* binding(uint32_t binding) const;

private:
    void forget();

    core_handle<DescriptorSetLayout> _handle;
    std::vector<VkDescriptorSetLayoutBinding> _bindings;

public:
    DescriptorSetLayout(DescriptorSetLayout const& other) = delete;
    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept = default;
    DescriptorSetLayout& operator=(DescriptorSetLayout const& other) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept = default;

    static constexpr void debug_check(DescriptorSetLayout const&);
};
}

namespace cvk {
struct DescriptorSetLayout::Config {
    /**
     * @brief adds a binding, the numbers may have gaps
     * @param binding number of the binding
     * @param count of descriptors, 0 -> the binding is reserved without descriptors
     * @pre no binding with the same number
     */
    constexpr Config& add(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages, uint32_t count = 1);

    /**
     * @brief removes a binding
     * @pre a binding with the number exists
     */
    constexpr Config& remove(uint32_t binding);

    /**
     * @brief bindings sorted by binding number
     */
    std::vector<VkDescriptorSetLayoutBinding> bindings{};
};

struct DescriptorSetLayout::State {
    vk_not_null<VkDescriptorSetLayout> vkLayout;
};
}

namespace cvk {
constexpr DescriptorSetLayout::Config& DescriptorSetLayout::Config::add(
    uint32_t binding,
    VkDescriptorType type,
    VkShaderStageFlags stages,
    uint32_t count
) {
    auto const it = std::ranges::lower_bound(bindings, binding, {}, &VkDescriptorSetLayoutBinding::binding);
    CTH_CRITICAL(it != bindings.end() && it->binding == binding, "binding {} already exists", binding) {}

    bindings.insert(
        it,
        VkDescriptorSetLayoutBinding{
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = count,
            .stageFlags = stages,
            .pImmutableSamplers = nullptr,
        }
    );
    return *this;
}

constexpr DescriptorSetLayout::Config& DescriptorSetLayout::Config::remove(uint32_t binding) {
    auto const it = std::ranges::lower_bound(bindings, binding, {}, &VkDescriptorSetLayoutBinding::binding);
    CTH_CRITICAL(it == bindings.end() || it->binding != binding, "binding {} does not exist", binding) {}

    bindings.erase(it);
    return *this;
}
}

namespace cvk {
constexpr void DescriptorSetLayout::debug_check(DescriptorSetLayout const& layout) {
    CTH_CRITICAL(!layout.created(), "descriptor set layout must be created") {}
}
}
