#pragma once
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>



namespace cth {
template<typename T>
span<T> Buffer<T>::map(const VkDeviceSize size, const VkDeviceSize offset) {
    const auto charSpan = DefaultBuffer::default_map(size * sizeof(T), offset * sizeof(T));
    return span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T> span<T> Buffer<T>::map() {
    const auto charSpan = default_map();
    return span<T>{reinterpret_cast<T*>(charSpan.data()), elements};
}
template<typename T>
void Buffer<T>::stage(span<const T> data, const VkDeviceSize buffer_offset) const {
    default_stage(span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)}, buffer_offset * sizeof(T));
}
template<typename T>
void Buffer<T>::write(span<const T> data, span<T> mapped_memory, const VkDeviceSize mapped_offset) const {
    const auto charData = span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    const auto charMapped = span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    default_write(charData, charMapped, mapped_offset * sizeof(T));
}
template<typename T> void Buffer<T>::write(span<const T> data, const VkDeviceSize mapped_offset) const {
    const auto charData = span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    default_write(charData, mapped_offset * sizeof(T));
}
template<typename T>
VkResult Buffer<T>::flush(const VkDeviceSize size, const VkDeviceSize offset) const {
    return DefaultBuffer::flush(size * sizeof(T), offset * sizeof(T));
}
template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(const VkDeviceSize size, const VkDeviceSize offset) const {
    if(size == VK_WHOLE_SIZE) return DefaultBuffer::descriptorInfo(VK_WHOLE_SIZE, 0);
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
    const VkMemoryPropertyFlags memory_property_flags) : DefaultBuffer(device, element_count * sizeof(T),
    usage_flags, memory_property_flags), elements(element_count) {}
} // namespace cth
