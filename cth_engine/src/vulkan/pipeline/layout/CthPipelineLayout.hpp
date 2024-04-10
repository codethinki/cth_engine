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


private:
    void create();

    Device* device;
    VkPipelineLayout vkLayout = VK_NULL_HANDLE;
    vector<DescriptorSetLayout*> setLayouts{};

public:
    /**
     * \throws cth::except::vk_result_exception data: VkResult of vkCreatePipelineLayout()
     * \throws cth::except::exception reason: device limits exceeded, too many locations specified
     */
    PipelineLayout(Device* device, const Builder& builder);
    ~PipelineLayout();

    [[nodiscard]] VkPipelineLayout get() const { return vkLayout; }

    struct Builder {
        Builder& addSetLayouts(span<DescriptorSetLayout* const> layouts, uint32_t location_offset = 0);
        Builder& addSetLayout(DescriptorSetLayout* layout, uint32_t location);
        Builder& removeSetLayout(uint32_t location);


        Builder() = default;
        explicit Builder(span<DescriptorSetLayout*> layouts);

    private:
        /**
         * \throws cth::except::exception reason: device limits exceeded, too many locations specified
         */
        [[nodiscard]] vector<DescriptorSetLayout*> build(uint32_t max_bound_descriptor_sets) const;

        vector<pair<uint32_t, DescriptorSetLayout*>> setLayouts{};

        friend PipelineLayout;
    };

    PipelineLayout(const PipelineLayout& other) = delete;
    PipelineLayout(PipelineLayout&& other) = delete;
    PipelineLayout& operator=(const PipelineLayout& other) = delete;
    PipelineLayout& operator=(PipelineLayout&& other) = delete;
};

}
