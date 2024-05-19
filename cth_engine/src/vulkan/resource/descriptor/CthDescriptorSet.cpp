#include "CthDescriptorSet.hpp"

#include "CthDescriptor.hpp"
#include "CthDescriptorPool.hpp"
#include "vulkan/render/pipeline/layout/CthDescriptorSetLayout.hpp"


#include <algorithm>
#include <cth/cth_log.hpp>

#include "vulkan/utility/CthVkUtils.hpp"



//DescriptorSet

namespace cth {
DescriptorSet::DescriptorSet(const Builder& builder) : _layout(builder._layout), _descriptors(builder._descriptors) { copyInfos(); }
DescriptorSet::~DescriptorSet() { if(_pool != nullptr) _pool->returnSet(this); }

void DescriptorSet::alloc(VkDescriptorSet set, DescriptorPool* pool) {
    _vkSet = set;
    this->_pool = pool;
}
void DescriptorSet::deallocate() {
    _vkSet = VK_NULL_HANDLE;
    _written = false;
    _pool = nullptr;
}


vector<VkWriteDescriptorSet> DescriptorSet::writes() {
    CTH_ERR(_vkSet == VK_NULL_HANDLE, "no descriptor set provided, call alloc() first")
        throw details->exception();

    _written = true;

    vector<VkWriteDescriptorSet> writes{};
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = _vkSet.get();

    for(auto [binding, binding_descriptors] : _descriptors | views::enumerate) {
        write.dstBinding = static_cast<uint32_t>(binding);
        write.descriptorType = _layout->bindingType(static_cast<uint32_t>(binding));
        const auto type = infoType(write.descriptorType);


        write.dstArrayElement = 0;
        write.descriptorCount = 0;

        for(auto [index, descriptor] : binding_descriptors | views::enumerate) {
            if(descriptor != nullptr) {
                write.descriptorCount++;
                continue;
            }

            //if no descriptor -> cannot write to it -> new write bc no "skip" placeholder exists for empty array index in binding.
            //ptr to bufferInfo = size - infoCount bc binding has only one type
            if(type == InfoType::BUFFER) write.pBufferInfo = &_bufferInfos[_bufferInfos.size() - write.descriptorCount];
            else write.pImageInfo = &_imageInfos[_imageInfos.size() - write.descriptorCount];

            //if write is usable -> push_back
            if(write.descriptorCount > 0) writes.push_back(write);

            write.dstArrayElement = static_cast<uint32_t>(index + 1); //next array index
            write.descriptorCount = 0; //reset descriptor count
        }

        if(write.descriptorCount == 0) continue;

        if(type == InfoType::BUFFER) write.pBufferInfo = &_bufferInfos[_bufferInfos.size() - write.descriptorCount];
        else write.pImageInfo = &_imageInfos[_imageInfos.size() - write.descriptorCount];

        writes.push_back(write);
    }

    return writes;
}

void DescriptorSet::copyInfos() {
    for(auto [binding, binding_descriptors] : _descriptors | views::enumerate) {
        const auto vkType = _layout->bindingType(static_cast<uint32_t>(binding));
        const auto type = infoType(vkType);

        CTH_WARN(binding_descriptors.empty(), "empty binding discovered")
            details->add("binding: {}", binding);

        CTH_STABLE_ASSERT(type != InfoType::NONE, "descriptor with no info not implemented") {
            details->add("binding: {}", binding);
            details->add("descriptor type: {}", utils::to_string(vkType));
        }


        for(auto [index, descriptor] : binding_descriptors | views::enumerate) {
            const bool empty = descriptor == nullptr;

            CTH_WARN(empty, "empty descriptor added") {
                details->add("binding: {}", binding);
                details->add("array index: {}", index);
            }

            if(empty) continue;

            if(type == InfoType::BUFFER) _bufferInfos.push_back(descriptor->bufferInfo());
            else _imageInfos.push_back(descriptor->imageInfo());
        }
    }
}

DescriptorSet::InfoType DescriptorSet::infoType(const VkDescriptorType descriptor_type) {
    switch(descriptor_type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return InfoType::BUFFER;

        case VK_DESCRIPTOR_TYPE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM:
        case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM:
            return InfoType::IMAGE;

        default:
            return InfoType::NONE;
    }
}

} // namespace cth


//Builder

namespace cth {
DescriptorSet::Builder::Builder(const DescriptorSetLayout* layout) : _layout(layout) { init(layout); }
DescriptorSet::Builder::Builder(const DescriptorSetLayout* layout, const span<Descriptor* const> descriptors, const uint32_t binding_offset) : _layout(layout) {
    init(layout);

    for(uint32_t i = 0; i < static_cast<uint32_t>(descriptors.size()); i++)
        addDescriptor(descriptors[i], binding_offset + i, 0);
}



DescriptorSet::Builder& DescriptorSet::Builder::addDescriptor(Descriptor* descriptor, uint32_t binding, const uint32_t arr_index) {

    CTH_ERR(descriptor != nullptr && (descriptor->type() != _layout->bindingType(binding)), "descriptor and layout type at binding dont match") {
        details->add("binding: {}", binding);
        details->add("descriptor type: {}", utils::to_string(descriptor->type()));
        details->add("layout type at binding: {}", utils::to_string(_layout->bindingType(binding)));

        throw cth::except::data_exception{_layout->bindingType(binding), details->exception()};
    }
    CTH_WARN(_descriptors[binding][arr_index] != nullptr, "overwriting already added descriptor") {
        details->add("binding: {}", binding);
        details->add("array index: {}", arr_index);
    }
    CTH_INFORM(descriptor == nullptr, "adding empty descriptor, consider using removeDescriptor() instead") {
        details->add("binding: {}", binding);
        details->add("array index: {}", arr_index);
    }


    _descriptors[binding][arr_index] = descriptor;
    return *this;
}
DescriptorSet::Builder& DescriptorSet::Builder::addDescriptors(const span<Descriptor* const> binding_descriptors, uint32_t binding, uint32_t arr_first) {
    CTH_ERR(_descriptors.size() + arr_first > _descriptors.size(), "out of range for layout size at binding") {
        details->add("binding: {0}, layout size: {1}", binding, _descriptors[binding].size());
        details->add("binding descriptors: {0}, arr_first: {1}", binding_descriptors.size(), arr_first);
        throw details->exception();
    }
    CTH_INFORM(ranges::any_of(binding_descriptors, [](Descriptor* descriptor) { return !descriptor; }),
        "adding empty descriptors, consider using removeDescriptors() instead") {
        details->add("binding: {}", binding);
        details->add("array first: {}", arr_first);
    }

    ranges::copy(binding_descriptors, _descriptors[binding].begin() + arr_first);

    return *this;
}
DescriptorSet::Builder& DescriptorSet::Builder::removeDescriptor(const uint32_t binding, const uint32_t arr_index) {
    _descriptors[binding][arr_index] = nullptr;
    return *this;
}
DescriptorSet::Builder& DescriptorSet::Builder::removeDescriptors(const uint32_t binding, const uint32_t arr_first, const uint32_t count) {
    CTH_ERR(arr_first + count > _descriptors[binding].size(), "out of ranger for layout size at binding") {
        details->add("binding: {0}, layout size: {1}", binding, _descriptors[binding].size());
        details->add("arr_first: {0}, count: {1}", arr_first, count);
        throw details->exception();
    }
    ranges::fill_n(_descriptors[binding].begin() + arr_first, count, nullptr);
    return *this;
}
void DescriptorSet::Builder::init(const DescriptorSetLayout* layout) {
    const auto& bindings = layout->bindingsVec();
    _descriptors.resize(bindings.size());

    ranges::for_each(bindings, [this](const VkDescriptorSetLayoutBinding& binding) { _descriptors[binding.binding].resize(binding.descriptorCount); });
}

}
