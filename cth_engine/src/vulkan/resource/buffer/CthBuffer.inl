#pragma once
#include "../CthDeletionQueue.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include "vulkan/resource/CthMemory.hpp"


namespace cth {

template<typename T>
Buffer<T>::Buffer(Device* device, DeletionQueue* deletion_queue, const size_t element_count, const VkBufferUsageFlags usage_flags,
    const VkMemoryPropertyFlags memory_property_flags) : BasicBuffer(device, element_count * sizeof(T), usage_flags,
    new Memory{device, deletion_queue, memory_property_flags}), _elements(element_count), deletionQueue(deletion_queue) {
    BasicBuffer::create();
    BasicBuffer::alloc();
    BasicBuffer::bind();
}
template<typename T> Buffer<T>::~Buffer() {
    Buffer<T>::destroy();
}

template<typename T>
span<T> Buffer<T>::map(const size_t size, const size_t offset) {
    const auto charSpan = BasicBuffer::map(size * sizeof(T), offset * sizeof(T));
    return span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T> span<T> Buffer<T>::map() {
    const auto charSpan = BasicBuffer::map();
    return span<T>{reinterpret_cast<T*>(charSpan.data()), _elements};
}
template<typename T>
void Buffer<T>::stage(const CmdBuffer* cmd_buffer, const Buffer<T>* staging_buffer, const size_t dst_offset) const {
    BasicBuffer::stage(cmd_buffer, staging_buffer, dst_offset * sizeof(T));
}
template<typename T>
void Buffer<T>::write(span<const T> data, span<T> mapped_memory) {
    const auto charData = span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    const auto charMapped = span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    BasicBuffer::write(charData, charMapped);
}
template<typename T>
void Buffer<T>::write(span<const T> data, const size_t mapped_offset) const {
    const auto charData = span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    BasicBuffer::write(charData, mapped_offset * sizeof(T));
}
template<typename T>
VkResult Buffer<T>::flush(const size_t size, const size_t offset) const { return BasicBuffer::flush(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
VkResult Buffer<T>::invalidate(const size_t size, const size_t offset) const { return BasicBuffer::invalidate(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
void Buffer<T>::copy(const CmdBuffer* cmd_buffer, const Buffer<T>* src, const size_t copy_size, const size_t src_offset, const size_t dst_offset) const {
    copy_size = copy_size == VK_WHOLE_SIZE ? VK_WHOLE_SIZE : copy_size * sizeof(T);
    BasicBuffer::copy(cmd_buffer, src, copy_size, src_offset * sizeof(T), dst_offset * sizeof(T));
}

template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(const size_t size, const size_t offset) const {
    if(size == VK_WHOLE_SIZE) return BasicBuffer::descriptorInfo(VK_WHOLE_SIZE, 0);
    return BasicBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}

template<typename T>
void Buffer<T>::destroy() {
    deletionQueue->enqueue(get());
    delete memory();


    reset();
}

} // namespace cth
