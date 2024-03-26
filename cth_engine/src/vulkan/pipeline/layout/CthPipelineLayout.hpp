#pragma once

#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

namespace cth {
using namespace std;
class Device;
class DescriptorSetLayout;

class PipelineLayout {
public:



private:
    void create();

    Device* device;
    VkPipelineLayout vkLayout = VK_NULL_HANDLE;
    vector<DescriptorSetLayout*> setLayouts{};

public:
    struct Builder {
        Builder& addSetLayouts(const vector<DescriptorSetLayout*>& layouts, uint32_t location_offset = 0);
        Builder& addSetLayout(DescriptorSetLayout* layout, uint32_t location);
        Builder& removeSetLayout(uint32_t location);


        Builder() = default;
        explicit Builder(const vector<DescriptorSetLayout*>& layouts);

    private:
        /**
         * \throws cth::except::exception reason: device limits exceeded, too many locations specified 
         */
        [[nodiscard]] vector<DescriptorSetLayout*> build(Device* device) const;

        vector<pair<uint32_t, DescriptorSetLayout*>> setLayouts{};

        friend PipelineLayout;
    };

    /**
     * \throws cth::except::vk_result_exception data: VkResult of vkCreatePipelineLayout()
     * * \throws cth::except::exception reason: device limits exceeded, too many locations specified
     */
    PipelineLayout(Device* device, const Builder& builder);
    ~PipelineLayout();

    [[nodiscard]] VkPipelineLayout get() const { return vkLayout; }
};

}
