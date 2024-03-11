#include "CthDefaultBuffer.hpp"

#include "../../core/CthDevice.hpp"
#include "../../utils/cth_vk_specific_utils.hpp"


namespace cth {

    VkDeviceSize DefaultBuffer::calcAlignedSize(const VkDeviceSize actual_size, const VkDeviceSize min_offset_alignment) {
        if(min_offset_alignment > 0) return actual_size + actual_size % min_offset_alignment;
        return actual_size;
    }
    span<char> DefaultBuffer::default_map(const VkDeviceSize size, const VkDeviceSize offset) {
        CTH_ERR(!vkBuffer || !vkMemory, "buffer not created yet") throw details->exception();
        CTH_ERR(size + offset > bufferSize - padding, "memory out of bounds")
            throw details->exception();

        void* mappedPtr = nullptr;
        const VkResult mapResult = vkMapMemory(device->device(), vkMemory, offset, size, 0, &mappedPtr);
        CTH_STABLE_ERR(mapResult != VK_SUCCESS, "Vk: memory mapping failed")
            throw except::vk_result_exception{mapResult, details->exception()};

        return span<char>{static_cast<char*>(mappedPtr), size};
    }
    span<char> DefaultBuffer::default_map() {
        CTH_ERR(mapped.data(), "buffer already mapped") throw details->exception();
        CTH_ERR(!vkBuffer || !vkMemory, "buffer not created yet") throw details->exception();

        void* mappedPtr = nullptr;
        const VkResult mapResult = vkMapMemory(device->device(), vkMemory, 0, VK_WHOLE_SIZE, 0, &mappedPtr);
        CTH_STABLE_ERR(mapResult != VK_SUCCESS, "Vk: memory mapping failed")
            throw except::vk_result_exception{mapResult, details->exception()};

        mapped = span<char>(static_cast<char*>(mappedPtr), bufferSize - padding);
        return mapped;
    }

    void DefaultBuffer::unmap() {
        if(!mapped.data()) return;

        vkUnmapMemory(device->device(), vkMemory);
        mapped = span<char>();
    }

    void DefaultBuffer::default_stage(const span<char> data, const VkDeviceSize buffer_offset) const {
        const unique_ptr<DefaultBuffer> stagingBuffer = make_unique<DefaultBuffer>(device, data.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        stagingBuffer->default_map();
        stagingBuffer->default_write(data);

        copyFromBuffer(stagingBuffer.get(), stagingBuffer->bufferSize, 0, buffer_offset);
    }

    void DefaultBuffer::default_write(const span<char> data, const span<char> mapped_memory, const VkDeviceSize mapped_offset) const {
        memcpy(mapped_memory.data() + mapped_offset, data.data(), data.size());
    }
    void DefaultBuffer::default_write(const span<char> data, const VkDeviceSize mapped_offset) const {
        CTH_ERR(!mapped.data(), "mapped_memory invalid or buffer was not mapped entirely")
            throw details->exception();

        memcpy(mapped.data() + mapped_offset, data.data(), data.size());
    }

    VkResult DefaultBuffer::flush(const VkDeviceSize size, const VkDeviceSize offset) const {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = vkMemory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(device->device(), 1, &mappedRange);
    }

    Descriptor::descriptor_info_t DefaultBuffer::descriptorInfo(const VkDeviceSize size, const VkDeviceSize offset) const {
        return VkDescriptorBufferInfo{vkBuffer, offset, size == VK_WHOLE_SIZE ? bufferSize : size};
    }

    VkResult DefaultBuffer::invalidate(const VkDeviceSize size, const VkDeviceSize offset) const {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = vkMemory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkInvalidateMappedMemoryRanges(device->device(), 1, &mappedRange);
    }
    void DefaultBuffer::copyFromBuffer(const DefaultBuffer* src, const VkDeviceSize size, const VkDeviceSize src_offset,
        const VkDeviceSize dst_offset) const {
        copyBuffer(src, this, size, src_offset, dst_offset);
    }
    void DefaultBuffer::copyBuffer(const DefaultBuffer* src, const DefaultBuffer* dst, const VkDeviceSize size, VkDeviceSize src_offset,
        VkDeviceSize dst_offset) {
        const VkDeviceSize copySize = size == VK_WHOLE_SIZE ? min(src->bufferSize, dst->bufferSize) : size;

        CTH_ERR(src_offset + copySize > src->bufferSize || dst_offset + copySize > dst->bufferSize, "copy operation out of bounds") {
            if(src_offset + copySize > src->bufferSize) {
                details->add("src buffer out of bounds");
                details->add("{0} + {1} > {2} (off + copy_size > src.size)", src_offset, copySize, src->bufferSize);
            }
            if(dst_offset + copySize > dst->bufferSize) {
                details->add("dst buffer out of bounds");
                details->add("{0} + {1} > {2} (off + copy_size > dst.size)", dst_offset, copySize, dst->bufferSize);
            }
            throw details->exception();
        }

        src->device->copyBuffer(src->vkBuffer, dst->vkBuffer, copySize, src_offset, dst_offset);
    }

    DefaultBuffer::DefaultBuffer(Device* device, const VkDeviceSize buffer_size, const VkBufferUsageFlags usage_flags,
        const VkMemoryPropertyFlags memory_property_flags, const VkDeviceSize min_offset_alignment) : device(device),
        bufferSize(calcAlignedSize(buffer_size, min_offset_alignment)), padding(buffer_size - bufferSize),
        vkUsageFlags(usage_flags), vkMemoryPropertyFlags(memory_property_flags) {
        device->createBuffer(bufferSize, usage_flags, memory_property_flags, vkBuffer, vkMemory);
    }

    DefaultBuffer::~DefaultBuffer() {
        unmap();
        vkDestroyBuffer(device->device(), vkBuffer, nullptr);
        vkFreeMemory(device->device(), vkMemory, nullptr);
    }


} //namespace cth
