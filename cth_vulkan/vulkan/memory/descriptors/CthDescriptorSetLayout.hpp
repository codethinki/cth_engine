#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace cth {
class Device;

using namespace std;
//TODO create a ShaderStageCollection class for managing shaders and the descriptor layout

class DescriptorSetLayout {
public:
    struct Builder {
        Builder() = default;
        Builder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags flags, uint32_t count = 1);
        Builder& removeBinding(uint32_t binding);

    private:
        vector<VkDescriptorSetLayoutBinding> bindings{};
        friend DescriptorSetLayout;
    };

private:
    Device* device;
    VkDescriptorSetLayout vkLayout = VK_NULL_HANDLE;
    vector<VkDescriptorSetLayoutBinding> vkBindings{};

public:
    /**
     * \brief creates a DescriptorSetLayout with the copied builder data
     * \throws cth::except::vk_result_exception data: VkResult of vkCreateDescriptorSetLayout()
     */
    explicit DescriptorSetLayout(Device* device, const Builder& builder);
    ~DescriptorSetLayout();

    [[nodiscard]] VkDescriptorSetLayout get() const { return vkLayout; }
    [[nodiscard]] uint32_t bindings() const { return static_cast<uint32_t>(vkBindings.size()); }
    [[nodiscard]] vector<VkDescriptorSetLayoutBinding> bindingsVec() const { return vkBindings; }
    [[nodiscard]] VkDescriptorSetLayoutBinding binding(const uint32_t binding) const { return vkBindings[binding]; }
    [[nodiscard]] VkDescriptorType bindingType(const uint32_t binding) const { return vkBindings[binding].descriptorType; }



    DescriptorSetLayout(const DescriptorSetLayout& other) = delete;
    DescriptorSetLayout(DescriptorSetLayout&& other) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout& other) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) = delete;
};
}
