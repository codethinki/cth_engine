#include "CthBuffer.hpp"

#include <cassert>
#include <cstring>
#include <cth/cth_log.hpp>
#include <glm/ext/scalar_uint_sized.hpp>


namespace cth {
VkDeviceSize DefaultBuffer::calcAlignedSize(const VkDeviceSize actual_size, const VkDeviceSize min_offset_alignment) {
    if(min_offset_alignment > 0) return actual_size + actual_size % min_offset_alignment;
    return actual_size;
}
span<char> cth::DefaultBuffer::map(const VkDeviceSize size, const VkDeviceSize offset) {
    CTH_ERR(vkBuffer && vkMemory, "buffer not created yet") throw details->exception();
    CTH_ERR(size + offset <= bufferSize - padding, "memory out of bounds")
        throw details->exception();

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(device->device(), vkMemory, offset, size, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult == VK_SUCCESS, "Vk: memory mapping failed")
        throw cth::except::data_exception{mapResult, details->exception()};

    return span<char>{static_cast<char*>(mappedPtr), size};
}
span<char> DefaultBuffer::map() {
    CTH_ERR(!mapped.data(), "buffer already mapped") throw details->exception();
    CTH_ERR(vkBuffer && vkMemory, "buffer not created yet") throw details->exception();

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(device->device(), vkMemory, 0, VK_WHOLE_SIZE, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult == VK_SUCCESS, "Vk: memory mapping failed")
        throw cth::except::data_exception{mapResult, details->exception()};

    mapped = span<char>(static_cast<char*>(mappedPtr), bufferSize - padding);
    return mapped;
}

void cth::DefaultBuffer::unmap() {
    if(!mapped.data()) return;

    vkUnmapMemory(device->device(), vkMemory);
    mapped = span<char>();
}

void DefaultBuffer::stage(const span<char> data, const VkDeviceSize buffer_offset) const {
    const unique_ptr<DefaultBuffer> stagingBuffer = make_unique<DefaultBuffer>(device, data.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer->map();
    stagingBuffer->writeToBuffer(data);

    copyFromBuffer(stagingBuffer.get(), stagingBuffer->bufferSize, 0, buffer_offset);
}

void DefaultBuffer::writeToBuffer(const span<char> data, const span<char> mapped_memory, const VkDeviceSize mapped_offset) const {
    memcpy(mapped_memory.data() + mapped_offset, data.data(), data.size());
}
void DefaultBuffer::writeToBuffer(const span<char> data, const VkDeviceSize mapped_offset) const {
    CTH_ERR(mapped.data(), "mapped_memory invalid or buffer was not mapped entirely")
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
VkDescriptorBufferInfo DefaultBuffer::descriptorInfo(const VkDeviceSize size, const VkDeviceSize offset) const {
    return VkDescriptorBufferInfo{vkBuffer, offset, size};
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
    const VkDeviceSize dst_offset) const { copyBuffer(src, this, size, src_offset, dst_offset); }
void DefaultBuffer::copyBuffer(const DefaultBuffer* src, const DefaultBuffer* dst, const VkDeviceSize size, VkDeviceSize src_offset,
    VkDeviceSize dst_offset) {
    const VkDeviceSize copySize = size == VK_WHOLE_SIZE ? min(src->bufferSize, dst->bufferSize) : size;

    CTH_ERR(src_offset + copySize <= src->bufferSize && dst_offset + copySize <= dst->bufferSize, "copy operation out of bounds") {
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
    const VkMemoryPropertyFlags memory_property_flags,
    const VkDeviceSize min_offset_alignment) : device(device), bufferSize(calcAlignedSize(buffer_size, min_offset_alignment)),
    padding(buffer_size - bufferSize),
    vkUsageFlags(usage_flags), vkMemoryPropertyFlags(memory_property_flags) {
    device->createBuffer(bufferSize, usage_flags, memory_property_flags, vkBuffer, vkMemory);
}

DefaultBuffer::~DefaultBuffer() {
    unmap();
    vkDestroyBuffer(device->device(), vkBuffer, nullptr);
    vkFreeMemory(device->device(), vkMemory, nullptr);
}


}

namespace cth {

template<typename T>
span<T> cth::Buffer<T>::map(const VkDeviceSize size, const VkDeviceSize offset) {
    const auto charSpan = DefaultBuffer::map(size * sizeof(T), offset * sizeof(T));
    return span<T>{static_cast<T*>(charSpan.data()), size};
}
template<typename T> span<T> Buffer<T>::map() {
    const auto charSpan = DefaultBuffer::map();
    return span<T>{static_cast<T*>(charSpan.data()), elements};
}
template<typename T>
void Buffer<T>::stage(span<T> data, const VkDeviceSize buffer_offset) const {
    DefaultBuffer::stage(span<char>{static_cast<char*>(data.data()), data.size() * sizeof(T)}, buffer_offset * sizeof(T));
}
template<typename T>
void Buffer<T>::writeToBuffer(span<T> data, span<T> mapped_memory, const VkDeviceSize mapped_offset) const {
    const auto charData = span<char>{static_cast<char*>(data.data()), data.size() * sizeof(T)};
    const auto charMapped = span<char>{static_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    DefaultBuffer::writeToBuffer(charData, charMapped, mapped_offset * sizeof(T));
}
template<typename T> void Buffer<T>::writeToBuffer(span<T> data, const VkDeviceSize mapped_offset) const {
    const auto charData = span<char>{static_cast<char*>(data.data()), data.size() * sizeof(T)};
    DefaultBuffer::writeToBuffer(charData, mapped_offset * sizeof(T));
}
template<typename T>
VkResult Buffer<T>::flush(const VkDeviceSize size, const VkDeviceSize offset) const {
    return DefaultBuffer::flush(size * sizeof(T), offset * sizeof(T));
}
template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(const VkDeviceSize size, const VkDeviceSize offset) const {
    return DefaultBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}
template<typename T>
VkResult Buffer<T>::invalidate(const VkDeviceSize size, const VkDeviceSize offset) const {
    return DefaultBuffer::invalidate(size * sizeof(T), offset * sizeof(T));
}
template<typename T>
void Buffer<T>::copyFromBuffer(const Buffer<T>& src, const VkDeviceSize size, const VkDeviceSize src_offset, const VkDeviceSize dst_offset) const {
    const auto srcBuffer = static_cast<DefaultBuffer*>(&src);
    const auto dstBuffer = static_cast<DefaultBuffer*>(this);

    DefaultBuffer::copyFromBuffer(srcBuffer, dstBuffer, size * sizeof(T), src_offset * sizeof(T), dst_offset * sizeof(T));
}
template<typename T> void Buffer<T>::copyBuffer(const Buffer<T>* src, const Buffer<T>* dst, const VkDeviceSize size, const VkDeviceSize src_offset,
    const VkDeviceSize dst_offset) {
    const auto srcBuffer = static_cast<DefaultBuffer*>(&src);
    const auto dstBuffer = static_cast<DefaultBuffer*>(&dst);

    DefaultBuffer::copyBuffer(srcBuffer, dstBuffer, size * sizeof(T), src_offset * sizeof(T), dst_offset * sizeof(T));
}
template<typename T> Buffer<T>::Buffer(Device* device, const VkDeviceSize element_count, const VkBufferUsageFlags usage_flags,
    const VkMemoryPropertyFlags memory_property_flags, const VkDeviceSize min_offset_alignment) : DefaultBuffer(device, element_count * sizeof(T),
        usage_flags, memory_property_flags, min_offset_alignment), elements(element_count) {}



//Buffer::Buffer(Device* device, const VkDeviceSize instance_size, const VkDeviceSize instance_count, const VkBufferUsageFlags usage_flags,
//    const VkMemoryPropertyFlags memory_property_flags, const VkDeviceSize min_offset_alignment) : device{device}, instanceSize{instance_size},
//    padding(calcPadding(instance_size, min_offset_alignment)), instances{instance_count},
//    bufferSize(instance_size * instance_count + padding), usageFlags{usage_flags},
//    memoryPropertyFlags{memory_property_flags} { device->createBuffer(bufferSize, usage_flags, memory_property_flags, vkBuffer, vkMemory); }
//
//Buffer::~Buffer() {
//    unmap();
//    vkDestroyBuffer(device->device(), vkBuffer, nullptr);
//    vkFreeMemory(device->device(), vkMemory, nullptr);
//}

}
