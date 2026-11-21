#include "cvk/data/descriptor/descriptor.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk::dev {
namespace {
    /**
     * @return the mapped cell of the heap
     */
    [[nodiscard]] VkHostAddressRangeEXT host_cell(
        BaseBuffer const& heap,
        size_t offset,
        size_t size
    ) {
        auto const mapped = heap.memory()->mapped();
        CTH_CRITICAL(mapped.empty(), "the heap memory must be mapped") {}
        CTH_CRITICAL(
            offset + size > mapped.size(),
            "the cell must lie in the heap, got {} + {} of {}",
            offset,
            size,
            mapped.size()
        ) {}

        return VkHostAddressRangeEXT{.address = mapped.data() + offset, .size = size};
    }

    /**
     * encodes the image descriptor of the type into the cell, then flushes the cell
     */
    void write_image(
        BaseBuffer const& heap,
        size_t offset,
        size_t size,
        VkDescriptorType type,
        Image const& image,
        ImageViewConfig const& view,
        VkImageLayout layout
    ) {
        BaseBuffer::debug_check(heap);
        ImageViewConfig::debug_check(view, image);

        auto const viewInfo = view.createInfo(image);
        VkImageDescriptorInfoEXT const imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
            .pNext = nullptr,
            .pView = &viewInfo,
            .layout = layout,
        };
        VkResourceDescriptorInfoEXT const info{
            .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
            .pNext = nullptr,
            .type = type,
            .data = {.pImage = &imageInfo},
        };
        auto const cell = host_cell(heap, offset, size);

        auto const& core = heap.core();
        auto const result = core.functions()->vkWriteResourceDescriptorsEXT(
            core.vkDevice(),
            1,
            &info,
            &cell
        );
        CVK_RESULT_STABLE_THROW(
            result != VK_SUCCESS,
            result,
            "failed to write the image descriptor"
        ) {}

        heap.flush({.offset = offset, .size = size});
    }
}

void debug_check_push_offset(
    [[maybe_unused]] Core const& core,
    [[maybe_unused]] uint32_t pushOffset
) {
    CTH_CRITICAL(
        pushOffset + 4 > core.physicalDevice().descriptorHeapProperties().maxPushDataSize,
        "the push index must lie in maxPushDataSize, got {} + 4 of {}",
        pushOffset,
        core.physicalDevice().descriptorHeapProperties().maxPushDataSize
    ) {}
}

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    SampledImageDescriptor const& descriptor
) {
    auto const& [image, view, layout] = descriptor;
    write_image(heap, offset, size, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, *image, view, layout);
}

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    StorageImageDescriptor const& descriptor
) {
    auto const& [image, view, layout] = descriptor;
    write_image(heap, offset, size, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, *image, view, layout);
}

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    InputAttachmentDescriptor const& descriptor
) {
    auto const& [image, view, layout] = descriptor;
    write_image(heap, offset, size, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, *image, view, layout);
}

void write_descriptor(
    BaseBuffer const& heap,
    size_t offset,
    size_t size,
    SamplerConfig const& sampler
) {
    BaseBuffer::debug_check(heap);

    auto const& core = heap.core();
    auto const limit = core.physicalDevice().limits().maxSamplerAnisotropy;
    CTH_WARN(
        sampler.maxAnisotropy > limit,
        "maxAnisotropy clamped to the device limit {}, got {}",
        limit,
        sampler.maxAnisotropy
    ) {}

    auto info = sampler.createInfo();
    info.maxAnisotropy = std::min(info.maxAnisotropy, limit);
    auto const cell = host_cell(heap, offset, size);

    auto const result = core.functions()->vkWriteSamplerDescriptorsEXT(
        core.vkDevice(),
        1,
        &info,
        &cell
    );
    CVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS,
        result,
        "failed to write the sampler descriptor"
    ) {}

    heap.flush({.offset = offset, .size = size});
}
}
