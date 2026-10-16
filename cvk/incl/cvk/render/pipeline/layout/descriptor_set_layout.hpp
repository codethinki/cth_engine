#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>
#include <cth/utility.hpp>

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

    static void destroy(DeviceTable table, VkDescriptorSetLayout);

    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkDescriptorSetLayout get() const { return _handle.get(); }
    [[nodiscard]] Core const& core() const { return *_core; }

    /**
     * @return bindings, sorted by binding number
     */
    [[nodiscard]] std::span<VkDescriptorSetLayoutBinding const> bindings() const;

    /**
     * @return nullptr -> no binding with that number
     */
    [[nodiscard]] VkDescriptorSetLayoutBinding const* binding(uint32_t binding) const;

private:
    void invalidate();

    not_null<Core const*> _core;
    std::vector<VkDescriptorSetLayoutBinding> _bindings;
    move_ptr<VkDescriptorSetLayout_T> _handle = VK_NULL_HANDLE;

public:
    DescriptorSetLayout(DescriptorSetLayout const& other) = delete;
    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept = default;
    DescriptorSetLayout& operator=(DescriptorSetLayout const& other) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept {
        return cth::reconstruct_at(*this, std::move(other));
    }

    static void debug_check(DescriptorSetLayout const&);
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
    Config& add(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages, uint32_t count = 1);

    /**
     * @brief removes a binding
     * @pre a binding with the number exists
     */
    Config& remove(uint32_t binding);

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
inline void DescriptorSetLayout::debug_check(DescriptorSetLayout const& layout) {
    CTH_CRITICAL(!layout.created(), "descriptor set layout must be created") {}
}
}
