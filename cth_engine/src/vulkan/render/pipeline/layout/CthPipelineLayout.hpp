#pragma once

#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace cth {
using namespace std;
class Device;
class DescriptorSetLayout;

class PipelineLayout {
public:
    struct Builder;
    /**
    * \throws cth::except::vk_result_exception data: VkResult of vkCreatePipelineLayout()
    * \throws cth::except::exception reason: device limits exceeded, too many locations specified
    */
    PipelineLayout(Device* device, const Builder& builder);
    ~PipelineLayout();

private:
    void create();

    Device* _device;
    VkPipelineLayout _vkLayout = VK_NULL_HANDLE;
    vector<DescriptorSetLayout*> _setLayouts{};

public:
    [[nodiscard]] VkPipelineLayout get() const { return _vkLayout; }



    PipelineLayout(const PipelineLayout& other) = delete;
    PipelineLayout(PipelineLayout&& other) = delete;
    PipelineLayout& operator=(const PipelineLayout& other) = delete;
    PipelineLayout& operator=(PipelineLayout&& other) = delete;
};

} // namespace cth

//Builder

namespace cth {
struct PipelineLayout::Builder {
    Builder() = default;
    explicit Builder(span<DescriptorSetLayout*> layouts);

    Builder& addSetLayouts(span<DescriptorSetLayout* const> layouts, uint32_t location_offset = 0);
    Builder& addSetLayout(DescriptorSetLayout* layout, uint32_t location);
    Builder& removeSetLayout(uint32_t location);

private:
    /**
     * \throws cth::except::exception reason: device limits exceeded, too many locations specified
     */
    [[nodiscard]] vector<DescriptorSetLayout*> build(uint32_t max_bound_descriptor_sets) const;

    vector<pair<uint32_t, DescriptorSetLayout*>> _setLayouts{};

    friend PipelineLayout;
};
} // namespace cth
