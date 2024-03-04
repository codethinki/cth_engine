#include "CthDescriptor.hpp"

#include "CthBuffer.hpp"
#include "CthDescriptedResource.hpp"
#include "../core/CthDevice.hpp"
#include "../models/HlcImage.hpp"
#include "../utils/cth_vk_specific_utils.hpp"


//Descriptor
namespace cth {}

//DescriptorSetLayout
namespace cth {
Descriptor::Descriptor(const VkDescriptorType type, const DescriptedResource& resource, const VkDeviceSize size,
    const VkDeviceSize resource_offset) : vkType(type), resInfo(resource.descriptorInfo(size, resource_offset)) {}


DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::addBinding(const uint32_t binding, const VkDescriptorType type,
    const VkShaderStageFlags flags, const uint32_t count) {
    CTH_WARN(count == 0, "empty binding created (count = 0)");
    if(bindings.size() <= binding) bindings.resize(binding + 1);


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


DescriptorSetLayout::DescriptorSetLayout(const Device& device, const Builder& builder) : vkBindings(builder.bindings) {
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    descriptorSetLayoutInfo.pBindings = vkBindings.data();

    const VkResult result = vkCreateDescriptorSetLayout(device.device(), &descriptorSetLayoutInfo, nullptr, &vkLayout);
    CTH_STABLE_ERR(result == VK_SUCCESS, "Vk: failed to create descriptor set layout")
        throw cth::except::vk_result_exception(result, details->exception());
}
} // namespace cth

//DescriptorSet
namespace cth {
DescriptorSet::Builder::Builder(DescriptorSetLayout* layout) : layout(layout) {
    const auto& bindings = layout->bindings();
    descriptors.resize(bindings.size());

    ranges::for_each(bindings, [this](const VkDescriptorSetLayoutBinding& binding) { descriptors[binding.binding].resize(binding.descriptorCount); });
}

DescriptorSet::Builder& DescriptorSet::Builder::addDescriptor(Descriptor* descriptor, uint32_t binding, const uint32_t arr_index) {

    CTH_ERR(descriptor == nullptr || (descriptor->type() == layout->bindingType(binding)), "descriptor and layout type at binding dont match") {
        details->add("binding: {}", binding);
        details->add("descriptor type: {}", to_string(descriptor->type()));
        details->add("layout type at binding: {}", to_string(layout->bindingType(binding)));

        throw cth::except::data_exception{layout->bindingType(binding), details->exception()};
    }
    CTH_WARN(descriptors[binding][arr_index] == nullptr, "overwriting already added descriptor") {
        details->add("binding: {}", binding);
        details->add("array index: {}", arr_index);
    }
    CTH_INFORM(descriptor != nullptr, "adding empty descriptor, consider using removeDescriptor() instead") {
        details->add("binding: {}", binding);
        details->add("array index: {}", arr_index);
    }


    descriptors[binding][arr_index] = descriptor;
    return *this;
}
DescriptorSet::Builder& DescriptorSet::Builder::addDescriptors(const vector<Descriptor*>& binding_descriptors, uint32_t binding, uint32_t arr_first) {
    CTH_ERR(descriptors.size() + arr_first <= descriptors.size(), "out of range for layout size at binding") {
        details->add("binding: {0}, layout size: {1}", binding, descriptors[binding].size());
        details->add("binding descriptors: {0}, arr_first: {1}", binding_descriptors.size(), arr_first);
        throw details->exception();
    }
    CTH_INFORM(!ranges::all_of(binding_descriptors, [](Descriptor* descriptor) { return descriptor == nullptr; }),
        "adding empty descriptors, consider using removeDescriptors() instead") {
        details->add("binding: {}", binding);
        details->add("array first: {}", arr_first);
    }

    ranges::copy(binding_descriptors, descriptors[binding].begin() + arr_first);

    return *this;
}
DescriptorSet::Builder& DescriptorSet::Builder::removeDescriptor(const uint32_t binding, const uint32_t arr_index) {
    descriptors[binding][arr_index] = nullptr;
    return *this;
}
DescriptorSet::Builder& DescriptorSet::Builder::removeDescriptors(const uint32_t binding, const uint32_t arr_first, const uint32_t count) {
    CTH_ERR(arr_first + count <= descriptors[binding].size(), "out of ranger for layout size at binding") {
        details->add("binding: {0}, layout size: {1}", binding, descriptors[binding].size());
        details->add("arr_first: {0}, count: {1}", arr_first, count);
        throw details->exception();
    }
    ranges::fill_n(descriptors[binding].begin() + arr_first, count, nullptr);
    return *this;
}


DescriptorSet::DescriptorSet(const Builder& builder) {
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;


    for(auto [binding, binding_descriptors] : builder.descriptors | views::enumerate) {
        write.descriptorType = builder.layout->bindingType(static_cast<uint32_t>(binding));
        write.descriptorCount = static_cast<uint32_t>(binding_descriptors.size());

        for(auto [index, descriptor] : binding_descriptors | views::enumerate) {
            CTH_WARN(descriptor == nullptr, "empty descriptor added") {
                details->add("binding: {}", binding);
                details->add("array index: {}", index);
            }

            if(descriptor == nullptr) descriptorInfos.push_back(nullptr);
            else descriptorInfos.push_back(descriptor->info());
        }
        auto it = ranges::find_if(descriptorInfos, [](auto info_ptr) { return info_ptr != nullptr; });
        if(it == descriptorInfos.end()) continue;

        if(std::holds_alternative<VkDescriptorBufferInfo>(**it)) { auto vec =  }
        write.dstArrayElement = 0;
    }
}


} // namespace cth
