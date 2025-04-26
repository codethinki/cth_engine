module;
#include <cth/io/io_log.hpp>

module cth.vk.res.desciptor.set_layout;

import cth.vk.exception;

import cth.io.log;

namespace cth::vk {

DescriptorSetLayout::DescriptorSetLayout(Core const& core, Builder const& builder) : _core{&core}, _vkBindings(builder.bindings()) { create(); }
DescriptorSetLayout::~DescriptorSetLayout() {
    _core->functions()->vkDestroyDescriptorSetLayout(_core->vkDevice(), _handle.get(), nullptr);
    log::msg("destroyed descriptor set layout");
}

void DescriptorSetLayout::create() {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(_vkBindings.size());
    descriptorSetLayoutInfo.pBindings = _vkBindings.data();

    VkDescriptorSetLayout ptr = VK_NULL_HANDLE;
    VkResult const result = _core->functions()->vkCreateDescriptorSetLayout(_core->vkDevice(), &descriptorSetLayoutInfo, nullptr, &ptr);
    CTH_STABLE_ERR(result != VK_SUCCESS, "Vk: failed to create descriptor set layout")
        throw cth::vk::result_exception(result, details->exception());

    _handle = ptr;

    log::msg("created descriptor set layout");
}

} // namespace cth


//Builder

namespace cth::vk {

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addBinding(uint32_t binding, VkDescriptorType type,
    VkShaderStageFlags flags, uint32_t count) {
    CTH_WARN(count == 0, "empty binding created (count = 0)") {}
    if(binding >= _bindings.size()) _bindings.resize(binding + 1);

    CTH_WARN(_bindings[binding].has_value(), "overwriting binding, consider using removeBinding first") {}

    VkDescriptorSetLayoutBinding layoutBinding;
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.stageFlags = flags;
    layoutBinding.descriptorCount = count;
    layoutBinding.pImmutableSamplers = nullptr;

    _bindings[binding] = layoutBinding;

    return *this;
}
DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::removeBinding(uint32_t binding) {
    _bindings[binding] = VkDescriptorSetLayoutBinding{};
    return *this;
}
std::vector<VkDescriptorSetLayoutBinding> DescriptorSetLayout::Builder::bindings() const {
    //TODO check this, may be possible
    CTH_CRITICAL(std::ranges::any_of(_bindings, [](const binding_t& binding){ return binding == std::nullopt;}), "bindings cannot be empty") {}

    std::vector<VkDescriptorSetLayoutBinding> vec(_bindings.size());
    std::ranges::transform(_bindings, vec.begin(), [](binding_t const& binding) { return binding.value(); });
    return vec;
}
}
