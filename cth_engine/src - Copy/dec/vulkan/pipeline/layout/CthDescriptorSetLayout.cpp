#include "CthDescriptorSetLayout.hpp"



#include "CthDescriptor.hpp"
#include "../../core/CthDevice.hpp"
#include "../../utils/cth_vk_specific_utils.hpp"



namespace cth {

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addBinding(const uint32_t binding, const VkDescriptorType type,
    const VkShaderStageFlags flags, const uint32_t count) {
    CTH_WARN(count == 0, "empty binding created (count = 0)");
    if(binding > bindings.size()) bindings.resize(binding + 1);


    VkDescriptorSetLayoutBinding& layoutBinding = bindings[binding];
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.stageFlags = flags;
    layoutBinding.descriptorCount = count;

    return *this;
}
DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::removeBinding(const uint32_t binding) {
    bindings[binding] = VkDescriptorSetLayoutBinding{};
    return *this;
}


DescriptorSetLayout::DescriptorSetLayout(Device* device, const Builder& builder) : device(device), vkBindings(builder.bindings) {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    descriptorSetLayoutInfo.pBindings = vkBindings.data();

    const VkResult result = vkCreateDescriptorSetLayout(device->device(), &descriptorSetLayoutInfo, nullptr, &vkLayout);
    CTH_STABLE_ERR(result != VK_SUCCESS, "Vk: failed to create descriptor set layout")
        throw cth::except::vk_result_exception(result, details->exception());
}
DescriptorSetLayout::~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(device->device(), vkLayout, nullptr);
}
} // namespace cth
