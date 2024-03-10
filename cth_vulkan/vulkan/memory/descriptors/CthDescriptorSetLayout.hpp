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

    /**
     * \brief creates a DescriptorSetLayout with the copied builder data
     */
    explicit DescriptorSetLayout(const Device& device, const Builder& builder);

private:
    VkDescriptorSetLayout vkLayout = VK_NULL_HANDLE;
    vector<VkDescriptorSetLayoutBinding> vkBindings{};

public:
    [[nodiscard]] VkDescriptorSetLayout get() const { return vkLayout; }
    [[nodiscard]] vector<VkDescriptorSetLayoutBinding> bindings() const { return vkBindings; }
    [[nodiscard]] VkDescriptorSetLayoutBinding binding(const uint32_t binding) const { return vkBindings[binding]; }
    [[nodiscard]] VkDescriptorType bindingType(const uint32_t binding) const { return vkBindings[binding].descriptorType; }



    DescriptorSetLayout(const DescriptorSetLayout& other) = default;
    DescriptorSetLayout(DescriptorSetLayout&& other) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout& other) = default;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) = delete;
};
}
