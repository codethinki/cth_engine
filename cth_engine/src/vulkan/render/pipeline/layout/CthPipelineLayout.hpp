#pragma once

#include <span>
#include <unordered_map>
#include <vector>
#include<cth/cth_pointer.hpp>
#include <vulkan/vulkan.h>



namespace cth::vk {
class BasicCore;
class DescriptorSetLayout;

class PipelineLayout {
public:
    struct Builder;
    /**
    * @throws cth::except::vk_result_exception data: VkResult of vkCreatePipelineLayout()
    * @throws cth::except::exception reason: device limits exceeded, too many locations specified
    */
    PipelineLayout(not_null<BasicCore const*> core, Builder const& builder);
    ~PipelineLayout();

private:
    void create();

    not_null<BasicCore const*> _core;
    VkPipelineLayout _vkLayout = VK_NULL_HANDLE;
    std::vector<DescriptorSetLayout*> _setLayouts{};

public:
    [[nodiscard]] VkPipelineLayout get() const { return _vkLayout; }



    PipelineLayout(PipelineLayout const& other) = delete;
    PipelineLayout(PipelineLayout&& other) = delete;
    PipelineLayout& operator=(PipelineLayout const& other) = delete;
    PipelineLayout& operator=(PipelineLayout&& other) = delete;
};

} // namespace cth

//Builder

namespace cth::vk {
struct PipelineLayout::Builder {
    Builder() = default;
    explicit Builder(std::span<DescriptorSetLayout*> layouts);

    Builder& addSetLayouts(std::span<DescriptorSetLayout* const> layouts, uint32_t location_offset = 0);
    Builder& addSetLayout(DescriptorSetLayout* layout, uint32_t location);
    Builder& removeSetLayout(uint32_t location);

private:
    /**
     * @throws cth::except::exception reason: device limits exceeded, too many locations specified
     */
    [[nodiscard]] std::vector<DescriptorSetLayout*> build(uint32_t max_bound_descriptor_sets) const;

    std::vector<std::pair<uint32_t, DescriptorSetLayout*>> _setLayouts{};

    friend PipelineLayout;
};
} // namespace cth
