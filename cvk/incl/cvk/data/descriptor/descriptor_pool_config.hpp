#pragma once
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>


namespace cvk {
/**
 * @brief set count, descriptor counts and flags of a descriptor pool
 */
struct DescriptorPoolConfig {
    /**
     * @brief count sets of one layout
     */
    struct Sets {
        not_null_ptr<DescriptorSetLayout const> layout;
        uint32_t count = 1;

        /**
         * @brief descriptors of the variable count binding per set (0 => its maximum count)
         */
        uint32_t variableCount = 0;
    };

    /**
     * @brief counts the sets and descriptors of the layouts
     * @param flags update after bind layouts add VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
     * @details calls @ref add() per entry
     */
    [[nodiscard]] static constexpr DescriptorPoolConfig ForLayouts(
        std::initializer_list<Sets> sets,
        VkDescriptorPoolCreateFlags flags = 0
    ) {
        DescriptorPoolConfig config{.flags = flags};
        for(auto const& [layout, count, variableCount] : sets)
            config.add(*layout, count, variableCount);
        return config;
    }

    /**
     * @pre > 0
     */
    uint32_t maxSets = 0;

    /**
     * @brief descriptors per type, for all sets of the pool together
     * @pre every count > 0
     */
    std::vector<VkDescriptorPoolSize> sizes{};

    /**
     * @brief VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT -> @ref DescriptorSet::destroy() frees the set,
     *  VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT -> sets of update after bind layouts
     */
    VkDescriptorPoolCreateFlags flags = 0;

    /**
     * @brief adds count sets of the layout to the set count and the descriptor counts
     * @param count of sets
     * @param variableCount descriptors of the variable count binding per set (0 => its maximum count)
     * @details update after bind layout -> adds VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
     * @pre variableCount != 0 -> the layout has a variable count binding with a maximum >= variableCount
     */
    constexpr DescriptorPoolConfig& add(DescriptorSetLayout const&, uint32_t count = 1, uint32_t variableCount = 0);

    /**
     * @attention pPoolSizes points into @ref sizes
     */
    [[nodiscard]] constexpr VkDescriptorPoolCreateInfo createInfo() const {
        return VkDescriptorPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .maxSets = maxSets,
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data(),
        };
    }

    static constexpr void debug_check(DescriptorPoolConfig const&);
};
}

namespace cvk {
constexpr DescriptorPoolConfig& DescriptorPoolConfig::add(
    DescriptorSetLayout const& layout,
    uint32_t count,
    uint32_t variableCount
) {
    auto const* variable = layout.variableBinding();
    CTH_CRITICAL(variableCount != 0 && variable == nullptr, "a variable count needs a variable count binding") {}
    CTH_CRITICAL(
        variable != nullptr && variableCount > variable->descriptorCount,
        "the variable count must not exceed the maximum count of the binding"
    ) {}

    maxSets += count;
    if(layout.updateAfterBind()) flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    for(auto const& binding : layout.bindings()) {
        uint32_t const descriptors = &binding == variable && variableCount != 0 ? variableCount
            : binding.descriptorCount;
        if(descriptors == 0) continue;

        auto const it = std::ranges::find(sizes, binding.descriptorType, &VkDescriptorPoolSize::type);
        if(it != sizes.end()) it->descriptorCount += descriptors * count;
        else sizes.push_back({binding.descriptorType, descriptors * count});
    }

    return *this;
}

constexpr void DescriptorPoolConfig::debug_check(DescriptorPoolConfig const& config) {
    CTH_CRITICAL(config.maxSets == 0, "a descriptor pool needs at least one set") {}
    CTH_CRITICAL(
        std::ranges::any_of(config.sizes, [](VkDescriptorPoolSize const& size) { return size.descriptorCount == 0; }),
        "descriptor counts must be > 0"
    ) {}
}
}
