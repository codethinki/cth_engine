#include "HlcDescriptor.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>



namespace cth {

// *************** Descriptor Set Layout Builder *********************

HlcDescriptorSetLayout::Builder& HlcDescriptorSetLayout::Builder::addBinding(const uint32_t binding, const VkDescriptorType descriptor_type,
    const VkShaderStageFlags stage_flags, const uint32_t count) {
    CTH_ASSERT(!bindings.contains(binding), "binding already in use");

    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = descriptor_type;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stage_flags;
    bindings[binding] = layoutBinding;
    return *this;
}

std::unique_ptr<HlcDescriptorSetLayout> HlcDescriptorSetLayout::Builder::build() const {
    return std::make_unique<HlcDescriptorSetLayout>(hlcDevice, bindings);
}

// *************** Descriptor Set Layout *********************

HlcDescriptorSetLayout::HlcDescriptorSetLayout(
    Device& hlc_device,
    const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings) : hlcDevice{hlc_device}, bindings{bindings} {
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};

    for(const auto& binding : bindings | views::values) setLayoutBindings.push_back(binding);

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

    const VkResult result = vkCreateDescriptorSetLayout(hlc_device.device(), &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout);
    CTH_STABLE_ERR(result != VK_SUCCESS, "Vk: failed to create descriptor set layout")
        throw cth::except::vk_result_exception(result, details->exception());
}

HlcDescriptorSetLayout::~HlcDescriptorSetLayout() { vkDestroyDescriptorSetLayout(hlcDevice.device(), descriptorSetLayout, nullptr); }

// *************** Descriptor Pool Builder *********************

HlcDescriptorPool::Builder& HlcDescriptorPool::Builder::addPoolSize(VkDescriptorType descriptor_type, uint32_t count) {
    poolSizes.emplace_back(descriptor_type, count);
    return *this;
}

HlcDescriptorPool::Builder& HlcDescriptorPool::Builder::setPoolFlags(const VkDescriptorPoolCreateFlags flags) {
    poolFlags = flags;
    return *this;
}
HlcDescriptorPool::Builder& HlcDescriptorPool::Builder::setMaxSets(const uint32_t count) {
    maxSets = count;
    return *this;
}

std::unique_ptr<HlcDescriptorPool> HlcDescriptorPool::Builder::build(VkDescriptorType descriptor_type, const uint32_t max_sets, uint32_t pool_size) {
    maxSets = max_sets;
    poolSizes.emplace_back(descriptor_type, pool_size);
    return std::make_unique<HlcDescriptorPool>(hlcDevice, maxSets, poolFlags, poolSizes);
}
std::unique_ptr<HlcDescriptorPool> HlcDescriptorPool::Builder::build() const {
    return std::make_unique<HlcDescriptorPool>(hlcDevice, maxSets, poolFlags, poolSizes);
}

// *************** Descriptor Pool *********************

HlcDescriptorPool::HlcDescriptorPool(Device& hlc_device,
    const uint32_t max_sets,
    const VkDescriptorPoolCreateFlags pool_flags,
    const std::vector<VkDescriptorPoolSize>& pool_sizes) : hlcDevice{hlc_device} {
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    descriptorPoolInfo.pPoolSizes = pool_sizes.data();
    descriptorPoolInfo.maxSets = max_sets;
    descriptorPoolInfo.flags = pool_flags;

    if(vkCreateDescriptorPool(hlc_device.device(), &descriptorPoolInfo, nullptr, &descriptorPool) !=
        VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool!");
}

HlcDescriptorPool::~HlcDescriptorPool() { vkDestroyDescriptorPool(hlcDevice.device(), descriptorPool, nullptr); }

bool HlcDescriptorPool::allocateDescriptorSet(
    const VkDescriptorSetLayout descriptor_set_layout,
    VkDescriptorSet& descriptor) const {   
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.pSetLayouts = &descriptor_set_layout;
    allocInfo.descriptorSetCount = 1;

    if(vkAllocateDescriptorSets(hlcDevice.device(), &allocInfo, &descriptor) != VK_SUCCESS) return false;
    return true;
}

void HlcDescriptorPool::freeDescriptors(const std::vector<VkDescriptorSet>& descriptors) const {
    vkFreeDescriptorSets(hlcDevice.device(), descriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
}

void HlcDescriptorPool::resetPool() const { vkResetDescriptorPool(hlcDevice.device(), descriptorPool, 0); }

// *************** Descriptor Writer *********************

HlcDescriptorWriter::HlcDescriptorWriter(HlcDescriptorSetLayout& set_layout, HlcDescriptorPool& pool) : setLayout{set_layout}, pool{pool} {}

HlcDescriptorWriter& HlcDescriptorWriter::writeBuffer(const uint32_t binding, const VkDescriptorBufferInfo* buffer_info) {
    assert(setLayout.bindings.contains(binding) && "Layout does not contain specified binding");

    const auto& bindingDescription = setLayout.bindings[binding];

    assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pBufferInfo = buffer_info;
    write.descriptorCount = 1;

    writes.push_back(write);
    return *this;
}

HlcDescriptorWriter& HlcDescriptorWriter::writeImage(const uint32_t binding, const VkDescriptorImageInfo* image_info) {
    assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");

    const auto& bindingDescription = setLayout.bindings[binding];

    assert(bindingDescription.descriptorCount == 1 && "Binding single descriptor info, but binding expects multiple");
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorType = bindingDescription.descriptorType;
    write.dstBinding = binding;
    write.pImageInfo = image_info;
    write.descriptorCount = 1;

    writes.push_back(write);
    return *this;
}

bool HlcDescriptorWriter::build(VkDescriptorSet& set) {
    if(!pool.allocateDescriptorSet(setLayout.getDescriptorSetLayout(), set)) return false;
    overwrite(set);
    return true;
}

void HlcDescriptorWriter::overwrite(const VkDescriptorSet& set) {
    for(auto& write : writes) write.dstSet = set;

    vkUpdateDescriptorSets(pool.hlcDevice.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

}
