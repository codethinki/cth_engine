#pragma once
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>



namespace cth {

template<typename T> Buffer<T>::Buffer(Device* device, const size_t element_count, const VkBufferUsageFlags usage_flags,
    const VkMemoryPropertyFlags memory_property_flags) : DefaultBuffer(device, element_count * sizeof(T),
    usage_flags, memory_property_flags), _elements(element_count) {}

template<typename T>
span<T> Buffer<T>::map(const size_t size, const size_t offset) {
    const auto charSpan = DefaultBuffer::default_map(size * sizeof(T), offset * sizeof(T));
    return span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T> span<T> Buffer<T>::map() {
    const auto charSpan = default_map();
    return span<T>{reinterpret_cast<T*>(charSpan.data()), _elements};
}
template<typename T>
void Buffer<T>::stage(span<const T> data, const size_t buffer_offset) const {
    default_stage(span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)}, buffer_offset * sizeof(T));
}
template<typename T>
void Buffer<T>::write(span<const T> data, span<T> mapped_memory, const size_t mapped_offset) const {
    const auto charData = span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    const auto charMapped = span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    default_write(charData, charMapped, mapped_offset * sizeof(T));
}
template<typename T>
void Buffer<T>::write(span<const T> data, const size_t mapped_offset) const {
    const auto charData = span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    default_write(charData, mapped_offset * sizeof(T));
}
template<typename T>
VkResult Buffer<T>::flush(const size_t size, const size_t offset) const { return DefaultBuffer::flush(size * sizeof(T), offset * sizeof(T)); }
template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(const size_t size, const size_t offset) const {
    if(size == VK_WHOLE_SIZE) return DefaultBuffer::descriptorInfo(VK_WHOLE_SIZE, 0);
    return DefaultBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}
template<typename T>
VkResult Buffer<T>::invalidate(const size_t size, const size_t offset) const {
    return DefaultBuffer::invalidate(size * sizeof(T), offset * sizeof(T));
}

template<typename T>
void Buffer<T>::copy(Buffer<T>* src, const size_t copy_size, const size_t src_offset, const size_t dst_offset) const {
    DefaultBuffer::copy(src, copy_size == VK_WHOLE_SIZE ? VK_WHOLE_SIZE : copy_size * sizeof(T), src_offset * sizeof(T), dst_offset * sizeof(T));
}

} // namespace cth
