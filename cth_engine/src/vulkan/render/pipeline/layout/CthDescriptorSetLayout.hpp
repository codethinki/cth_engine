#pragma once

#include <optional>
#include <vulkan/vulkan.h>

#include <vector>
#include <cth/cth_memory.hpp>

//TEMP add basic variant without raii
namespace cth {
class BasicCore;

using namespace std;
//TODO create a ShaderStageCollection class for managing shaders and the descriptor layout

class DescriptorSetLayout {
public:
    struct Builder;
    /**
    * \brief creates a DescriptorSetLayout with the copied builder data
    * \throws cth::except::vk_result_exception data: VkResult of vkCreateDescriptorSetLayout()
    */
    explicit DescriptorSetLayout(const BasicCore* core, const Builder& builder);
    ~DescriptorSetLayout();

private:
    void create();

    const BasicCore* _core;
    mem::basic_ptr<VkDescriptorSetLayout_T> _handle = VK_NULL_HANDLE;
    vector<VkDescriptorSetLayoutBinding> _vkBindings{};

public:
    [[nodiscard]] VkDescriptorSetLayout get() const { return _handle.get(); }
    [[nodiscard]] uint32_t bindings() const { return static_cast<uint32_t>(_vkBindings.size()); }
    [[nodiscard]] vector<VkDescriptorSetLayoutBinding> bindingsVec() const { return _vkBindings; }
    [[nodiscard]] VkDescriptorSetLayoutBinding binding(const uint32_t binding) const { return _vkBindings[binding]; }
    [[nodiscard]] VkDescriptorType bindingType(const uint32_t binding) const { return _vkBindings[binding].descriptorType; }

    DescriptorSetLayout(const DescriptorSetLayout& other) = delete;
    DescriptorSetLayout(DescriptorSetLayout&& other) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout& other) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) = delete;
};
} // namespace cth

//Builder

namespace cth {
struct DescriptorSetLayout::Builder {
    Builder() = default;
    Builder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags flags, uint32_t count = 1);
    Builder& removeBinding(uint32_t binding);

private:
#ifdef _DEBUG
    using binding_t = optional<VkDescriptorSetLayoutBinding>;
#else
    using binding_t = VkDescriptorSetLayoutBinding;
#endif

    vector<binding_t> _bindings{};
    [[nodiscard]] vector<VkDescriptorSetLayoutBinding> bindings() const;

    friend DescriptorSetLayout;
};
}
