#include "cvk/data/descriptor/descriptor_set.hpp"

#include "cvk/base/core.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <span>
#include <variant>
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

    /**
     * @return the layout of a view in a descriptor of the type
     */
    constexpr VkImageLayout descriptor_layout(VkDescriptorType type) {
        switch(type) {
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return VK_IMAGE_LAYOUT_GENERAL;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR;
            default: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    void debug_check_image(ImageView const* view, Sampler const* sampler, VkDescriptorType type) {
        bool const samples = type == VK_DESCRIPTOR_TYPE_SAMPLER || type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        CTH_CRITICAL(samples && sampler == nullptr, "sampler descriptors need a sampler") {}
        CTH_CRITICAL(!samples && sampler != nullptr, "the descriptor type takes no sampler") {}
        CTH_CRITICAL(type != VK_DESCRIPTOR_TYPE_SAMPLER && view == nullptr, "image descriptors need a view") {}
        if(view != nullptr) ImageView::debug_check(*view);
        if(sampler != nullptr) Sampler::debug_check(*sampler);
    }

    void debug_check_buffer(BaseBuffer const& buffer, Range range) {
        BaseBuffer::debug_check(buffer);
        CTH_CRITICAL(range.offset > buffer.size(), "offset must lie in the buffer") {}
        CTH_CRITICAL(
            range.size != VK_WHOLE_SIZE && range.size > buffer.size() - range.offset,
            "range must lie in the buffer"
        ) {}
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

    // reserved: the writes point into the infos
    std::vector<VkDescriptorImageInfo> imageInfos{};
    imageInfos.reserve(writes.size());
    std::vector<VkDescriptorBufferInfo> bufferInfos{};
    bufferInfos.reserve(writes.size());
    std::vector<VkWriteDescriptorSet> vkWrites{};
    vkWrites.reserve(writes.size());

    for(auto const& write : writes) {
        auto const* binding = _layout->binding(write._binding);
        CTH_CRITICAL(binding == nullptr, "the layout has no binding {}", write._binding) {}

        auto const type = binding->descriptorType;
        auto const kind = descriptor_kind(type);
        auto const* buffer = std::get_if<DescriptorWrite::BufferResource>(&write._resource);

        CTH_CRITICAL(
            kind == DescriptorKind::UNSUPPORTED,
            "descriptor type {} of binding {} not supported",
            type,
            write._binding
        ) {}
        CTH_CRITICAL(
            (kind == DescriptorKind::BUFFER) != (buffer != nullptr),
            "the kind of the write must match the type of the binding, got binding {} of type {}",
            write._binding,
            type
        ) {}
        CTH_CRITICAL(
            write._element >= capacity(*binding),
            "the element must lie in the binding, got element {} of binding {}",
            write._element,
            write._binding
        ) {}

        VkWriteDescriptorSet vkWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = get(),
            .dstBinding = write._binding,
            .dstArrayElement = write._element,
            .descriptorCount = 1,
            .descriptorType = type,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        if(buffer != nullptr) {
            debug_check_buffer(*buffer->buffer, buffer->range);

            auto const [offset, size] = buffer->range;
            bufferInfos.push_back(
                VkDescriptorBufferInfo{
                    .buffer = buffer->buffer->get(),
                    .offset = offset,
                    .range = size == VK_WHOLE_SIZE ? buffer->buffer->size() - offset : size,
                }
            );
            vkWrite.pBufferInfo = &bufferInfos.back();
        } else {
            auto const& [view, sampler, layout] = std::get<DescriptorWrite::ImageResource>(write._resource);
            debug_check_image(view, sampler, type);

            imageInfos.push_back(
                VkDescriptorImageInfo{
                    .sampler = sampler != nullptr ? sampler->get() : VK_NULL_HANDLE,
                    .imageView = view != nullptr ? view->get() : VK_NULL_HANDLE,
                    .imageLayout = view == nullptr ? VK_IMAGE_LAYOUT_UNDEFINED
                        : layout != VK_IMAGE_LAYOUT_MAX_ENUM ? layout
                        : descriptor_layout(type),
                }
            );
            vkWrite.pImageInfo = &imageInfos.back();
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
