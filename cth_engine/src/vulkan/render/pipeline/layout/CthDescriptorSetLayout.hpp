#pragma once
#include "vulkan/utility/cth_constants.hpp"

#include<cth/pointers.hpp>
#include <vulkan/vulkan.h>

#include <optional>
#include <vector>


namespace cth::vk {
class Core;

//TODO create a ShaderStageCollection class for managing shaders and the descriptor layout

class DescriptorSetLayout {
public:
    struct Builder;
    /**
    * @brief creates a DescriptorSetLayout with the copied builder data
    * @throws cth::vk::result_exception data: VkResult of @ref vkCreateDescriptorSetLayout()
    */
    explicit DescriptorSetLayout(cth::not_null<Core const*> core, Builder const& builder);
    ~DescriptorSetLayout();

private:
    void create();

    cth::not_null<Core const*> _core;
    move_ptr<VkDescriptorSetLayout_T> _handle = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayoutBinding> _vkBindings{};

public:
    [[nodiscard]] VkDescriptorSetLayout get() const { return _handle.get(); }
    [[nodiscard]] uint32_t bindings() const { return static_cast<uint32_t>(_vkBindings.size()); }
    [[nodiscard]] std::vector<VkDescriptorSetLayoutBinding> bindingsVec() const { return _vkBindings; }
    [[nodiscard]] VkDescriptorSetLayoutBinding binding(uint32_t binding) const { return _vkBindings[binding]; }
    [[nodiscard]] VkDescriptorType bindingType(uint32_t binding) const { return _vkBindings[binding].descriptorType; }

    DescriptorSetLayout(DescriptorSetLayout const& other) = delete;
    DescriptorSetLayout(DescriptorSetLayout&& other) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout const& other) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) = delete;
};
} // namespace cth

//Builder

namespace cth::vk {
struct DescriptorSetLayout::Builder {
    Builder() = default;
    Builder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags flags, uint32_t count = 1);
    Builder& removeBinding(uint32_t binding);

private:
#ifdef CONSTANT_DEBUG_MODE
    using binding_t = std::optional<VkDescriptorSetLayoutBinding>;
#else
    using binding_t = VkDescriptorSetLayoutBinding;
#endif

    std::vector<binding_t> _bindings{};
    [[nodiscard]] std::vector<VkDescriptorSetLayoutBinding> bindings() const;

    friend DescriptorSetLayout;
};
}
