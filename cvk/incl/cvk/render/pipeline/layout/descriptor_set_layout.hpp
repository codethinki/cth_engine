#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <vector>


namespace cvk {
class Core;

/**
 * @brief VkDescriptorSetLayout wrapper
 * @details a binding with VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT -> the layout has
 *  VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT
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
     * @pre @ref Config::debug_check() of the config
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
    [[nodiscard]] constexpr std::span<VkDescriptorSetLayoutBinding const> bindings() const { return _bindings; }

    /**
     * @return nullptr -> no binding with that number
     */
    [[nodiscard]] constexpr VkDescriptorSetLayoutBinding const* binding(uint32_t binding) const;

    /**
     * @return flags of the binding, no binding with that number -> 0
     */
    [[nodiscard]] constexpr VkDescriptorBindingFlags bindingFlags(uint32_t binding) const;

    /**
     * @return a binding has VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
     */
    [[nodiscard]] constexpr bool updateAfterBind() const;

    /**
     * @return binding with VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, nullptr -> none
     */
    [[nodiscard]] constexpr VkDescriptorSetLayoutBinding const* variableBinding() const;

private:
    void forget();

    core_handle<DescriptorSetLayout> _handle;
    std::vector<VkDescriptorSetLayoutBinding> _bindings;
    std::vector<VkDescriptorBindingFlags> _bindingFlags;

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
     * @param count of descriptors, 0 -> the binding is reserved without descriptors,
     *  VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT -> the maximum count
     * @param flags VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, ...
     * @pre no binding with the same number
     */
    constexpr Config& add(
        uint32_t binding,
        VkDescriptorType type,
        VkShaderStageFlags stages,
        uint32_t count = 1,
        VkDescriptorBindingFlags flags = 0
    );

    /**
     * @brief removes a binding
     * @pre a binding with the number exists
     */
    constexpr Config& remove(uint32_t binding);

    /**
     * @brief bindings sorted by binding number
     */
    std::vector<VkDescriptorSetLayoutBinding> bindings{};

    /**
     * @brief flags per binding in the order of @ref bindings, empty -> no flags
     */
    std::vector<VkDescriptorBindingFlags> bindingFlags{};

    static constexpr void debug_check(Config const&);
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
    uint32_t count,
    VkDescriptorBindingFlags flags
) {
    auto const it = std::ranges::lower_bound(bindings, binding, {}, &VkDescriptorSetLayoutBinding::binding);
    CTH_CRITICAL(it != bindings.end() && it->binding == binding, "binding {} already exists", binding) {}

    // flags are stored for every binding once one binding has flags
    if(flags != 0 || !bindingFlags.empty()) {
        auto const index = it - bindings.begin();
        bindingFlags.resize(bindings.size(), 0);
        bindingFlags.insert(bindingFlags.begin() + index, flags);
    }

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

    if(!bindingFlags.empty()) bindingFlags.erase(bindingFlags.begin() + (it - bindings.begin()));
    bindings.erase(it);
    return *this;
}
}

namespace cvk {
constexpr void DescriptorSetLayout::Config::debug_check(Config const& config) {
    CTH_CRITICAL(
        !config.bindingFlags.empty() && config.bindingFlags.size() != config.bindings.size(),
        "binding flags must be empty or one per binding"
    ) {}
    if(config.bindingFlags.empty()) return;

    for(auto const [index, flags] : config.bindingFlags | std::views::enumerate) {
        auto const& binding = config.bindings[index];
        bool const dynamic = binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
            || binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

        CTH_CRITICAL(
            (flags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0
            && static_cast<size_t>(index) + 1 != config.bindings.size(),
            "only the binding with the highest number can have a variable descriptor count, got binding {}",
            binding.binding
        ) {}
        CTH_CRITICAL(
            dynamic && (flags & (VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)) != 0,
            "dynamic buffer bindings cannot update after bind or have a variable descriptor count, got binding {}",
            binding.binding
        ) {}
    }
}

constexpr VkDescriptorSetLayoutBinding const* DescriptorSetLayout::binding(uint32_t binding) const {
    auto const it = std::ranges::lower_bound(_bindings, binding, {}, &VkDescriptorSetLayoutBinding::binding);
    return it != _bindings.end() && it->binding == binding ? std::to_address(it) : nullptr;
}

constexpr VkDescriptorBindingFlags DescriptorSetLayout::bindingFlags(uint32_t binding) const {
    if(_bindingFlags.empty()) return 0;

    auto const it = std::ranges::lower_bound(_bindings, binding, {}, &VkDescriptorSetLayoutBinding::binding);
    return it != _bindings.end() && it->binding == binding ? _bindingFlags[it - _bindings.begin()] : 0;
}

constexpr bool DescriptorSetLayout::updateAfterBind() const {
    return std::ranges::any_of(_bindingFlags, [](VkDescriptorBindingFlags flags) {
        return (flags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0;
    });
}

constexpr VkDescriptorSetLayoutBinding const* DescriptorSetLayout::variableBinding() const {
    if(_bindingFlags.empty() || (_bindingFlags.back() & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) == 0)
        return nullptr;
    return &_bindings.back();
}

constexpr void DescriptorSetLayout::debug_check(DescriptorSetLayout const& layout) {
    CTH_CRITICAL(!layout.created(), "descriptor set layout must be created") {}
}
}
