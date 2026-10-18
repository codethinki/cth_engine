#include "cvk/data/descriptor/descriptor_set.hpp"

#include "cvk/base/core.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <vector>


namespace cvk {
using std::span;

namespace {
    enum class DescriptorKind {
        IMAGE,
        BUFFER,
        UNSUPPORTED,
    };

    constexpr DescriptorKind descriptor_kind(VkDescriptorType type) {
        switch(type) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return DescriptorKind::IMAGE;
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return DescriptorKind::BUFFER;
            default: return DescriptorKind::UNSUPPORTED;
        }
    }

    constexpr bool dynamic_buffer(VkDescriptorType type) {
        return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC || type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    }

    void debug_check_image(ImageDescriptor const& descriptor, VkDescriptorType type) {
        CTH_CRITICAL(
            (type == VK_DESCRIPTOR_TYPE_SAMPLER || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            && descriptor.sampler == nullptr,
            "sampler descriptors need a sampler"
        ) {}
        CTH_CRITICAL(
            type != VK_DESCRIPTOR_TYPE_SAMPLER && descriptor.view == nullptr,
            "image descriptors need a view"
        ) {}
        CTH_CRITICAL(
            type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE && descriptor.layout != VK_IMAGE_LAYOUT_GENERAL,
            "storage image descriptors need VK_IMAGE_LAYOUT_GENERAL"
        ) {}
        if(descriptor.view != nullptr) ImageView::debug_check(*descriptor.view);
        if(descriptor.sampler != nullptr) Sampler::debug_check(*descriptor.sampler);
    }
}

DescriptorSet::DescriptorSet(DescriptorPool const& pool, DescriptorSetLayout const& layout, uint32_t variableCount) :
    _handle{pool}, _layout{&layout}, _variableCount{variableCount} {
    DescriptorPool::debug_check(pool);
    DescriptorSetLayout::debug_check(layout);

    auto const* variable = layout.variableBinding();
    CTH_CRITICAL(variableCount != 0 && variable == nullptr, "a variable count needs a variable count binding") {}
    CTH_CRITICAL(
        variable != nullptr && variableCount > variable->descriptorCount,
        "the variable count must not exceed the maximum count of the binding"
    ) {}
}

DescriptorSet::DescriptorSet(
    DescriptorPool const& pool,
    DescriptorSetLayout const& layout,
    uint32_t variableCount,
    State const& state
) : DescriptorSet{pool, layout, variableCount} { wrap(state); }

DescriptorSet::DescriptorSet(
    DescriptorPool const& pool,
    DescriptorSetLayout const& layout,
    uint32_t variableCount,
    create_t
) : DescriptorSet{pool, layout, variableCount} { create(); }

void DescriptorSet::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkSet.get(), pool());
    pool().add();
}

void DescriptorSet::create() {
    DescriptorPool::debug_check(pool());
    CTH_CRITICAL(
        _layout->updateAfterBind() && (pool().config().flags & VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT) == 0,
        "update after bind layouts need a pool with VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT"
    ) {}

    optDestroy();

    auto const* variable = _layout->variableBinding();
    uint32_t const variableCount = variable != nullptr ? capacity(*variable) : 0;
    VkDescriptorSetVariableDescriptorCountAllocateInfo const variableInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &variableCount,
    };

    VkDescriptorSetLayout const vkLayout = _layout->get();
    VkDescriptorSetAllocateInfo const info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = variable != nullptr ? &variableInfo : nullptr,
        .descriptorPool = pool().get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &vkLayout,
    };

    VkDescriptorSet ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkAllocateDescriptorSets(core().vkDevice(), &info, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to allocate descriptor set") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, pool());
    pool().add();
}

void DescriptorSet::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

DescriptorSet::State DescriptorSet::release() {
    debug_check(*this);

    State const state{_handle.release()};
    pool().remove();

    forget();
    return state;
}

void DescriptorSet::write(span<DescriptorWrite const> writes) const {
    debug_check(*this);

    size_t imageCount = 0;
    size_t bufferCount = 0;
    for(auto const& write : writes) {
        imageCount += write.images.size();
        bufferCount += write.buffers.size();
    }

    // reserved: the writes point into the infos
    std::vector<VkDescriptorImageInfo> imageInfos{};
    imageInfos.reserve(imageCount);
    std::vector<VkDescriptorBufferInfo> bufferInfos{};
    bufferInfos.reserve(bufferCount);
    std::vector<VkWriteDescriptorSet> vkWrites{};
    vkWrites.reserve(writes.size());

    for(auto const& write : writes) {
        auto const* binding = _layout->binding(write.binding);
        CTH_CRITICAL(binding == nullptr, "the layout has no binding {}", write.binding) {}

        auto const type = binding->descriptorType;
        auto const kind = descriptor_kind(type);
        auto const count = static_cast<uint32_t>(kind == DescriptorKind::IMAGE ? write.images.size()
            : write.buffers.size());

        CTH_CRITICAL(kind == DescriptorKind::UNSUPPORTED, "descriptor type not supported") {
            details->add("binding: {}, type: {}", write.binding, type);
        }
        CTH_CRITICAL(
            (kind == DescriptorKind::IMAGE && !write.buffers.empty())
            || (kind == DescriptorKind::BUFFER && !write.images.empty()),
            "the descriptors must match the type of the binding"
        ) {
            details->add("binding: {}, type: {}", write.binding, type);
        }
        CTH_CRITICAL(count == 0, "a write needs descriptors") {}
        CTH_CRITICAL(write.arrayElement + count > capacity(*binding), "the descriptors must lie in the binding") {
            details->add("binding: {}, first: {}, count: {}", write.binding, write.arrayElement, count);
        }

        VkWriteDescriptorSet vkWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = get(),
            .dstBinding = write.binding,
            .dstArrayElement = write.arrayElement,
            .descriptorCount = count,
            .descriptorType = type,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        if(kind == DescriptorKind::IMAGE) {
            vkWrite.pImageInfo = imageInfos.data() + imageInfos.size();
            for(auto const& descriptor : write.images) {
                debug_check_image(descriptor, type);
                imageInfos.push_back(descriptor.info());
            }
        } else {
            vkWrite.pBufferInfo = bufferInfos.data() + bufferInfos.size();
            for(auto const& descriptor : write.buffers)
                bufferInfos.push_back(descriptor.info());
        }

        vkWrites.push_back(vkWrite);
    }

    core().functions()->vkUpdateDescriptorSets(
        core().vkDevice(),
        static_cast<uint32_t>(vkWrites.size()),
        vkWrites.data(),
        0,
        nullptr
    );
}

void DescriptorSet::bind(
    BaseCmdBuffer const& cmdBuffer,
    PipelineLayout const& layout,
    uint32_t setIndex,
    VkPipelineBindPoint bindPoint,
    span<uint32_t const> dynamicOffsets
) const {
    debug_check(*this);
    BaseCmdBuffer::debug_check(cmdBuffer);
    CTH_CRITICAL(!cmdBuffer.recording(), "command buffer must be recording") {}
    PipelineLayout::debug_check(layout);
    CTH_CRITICAL(setIndex >= layout.setLayouts().size(), "the pipeline layout has no set {}", setIndex) {}

    auto const dynamicCount = std::ranges::fold_left(
        _layout->bindings(),
        uint32_t{0},
        [](uint32_t sum, VkDescriptorSetLayoutBinding const& binding) {
            return dynamic_buffer(binding.descriptorType) ? sum + binding.descriptorCount : sum;
        }
    );
    CTH_CRITICAL(dynamicOffsets.size() != dynamicCount, "one dynamic offset per dynamic buffer descriptor") {
        details->add("dynamic descriptors: {}, offsets: {}", dynamicCount, dynamicOffsets.size());
    }

    VkDescriptorSet const vkSet = get();
    core().functions()->vkCmdBindDescriptorSets(
        cmdBuffer.get(),
        bindPoint,
        layout.get(),
        setIndex,
        1,
        &vkSet,
        static_cast<uint32_t>(dynamicOffsets.size()),
        dynamicOffsets.data()
    );
}

void DescriptorSet::destroy(DeviceTable table, VkDescriptorPool vkPool, VkDescriptorSet vkSet) {
    CTH_WARN(vkSet == VK_NULL_HANDLE, "vkSet invalid (VK_NULL_HANDLE)") {}

    auto const result = table->vkFreeDescriptorSets(table.device(), vkPool, 1, &vkSet);
    CTH_WARN(result != VK_SUCCESS, "failed to free descriptor set") {}
}

void DescriptorSet::forget() {}

void DescriptorSet::pool_deleter::operator()(VkDescriptorSet vkSet) const {
    // a reset of the pool freed the set already
    if(_resets != _pool->resets()) return;

    _pool->remove();
    if(_pool->freesSets()) destroy(_pool->core().deviceTable(), _pool->get(), vkSet);
}
}
