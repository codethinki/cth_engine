#pragma once
#include "../CthDestructionQueue.hpp"
#include "vulkan/base/CthCore.hpp"


namespace cth::vk {

template<class T>
Buffer<T>::Buffer(cth::not_null<Core const*> core, size_t element_count, VkBufferUsageFlags usage_flags) :
    BaseBuffer{core, element_count * sizeof(T), usage_flags}, _elements(element_count) {}

template<class T>
Buffer<T>::Buffer(cth::not_null<Core const*> core, size_t element_count, VkBufferUsageFlags usage_flags, State state) :
    Buffer{core, element_count, usage_flags} { BaseBuffer::wrap(std::move(state)); }

template<class T>
Buffer<T>::Buffer(cth::not_null<Core const*> core, size_t element_count, VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags memory_property_flags) : Buffer{core, element_count * sizeof(T), usage_flags} {
    BaseBuffer::create(memory_property_flags);
}


template<typename T>
std::span<T> Buffer<T>::map(size_t size, size_t offset) {
    auto const charSpan = BaseBuffer::map(size * sizeof(T), offset * sizeof(T));
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T>
std::span<T> Buffer<T>::map() {
    auto const charSpan = BaseBuffer::map();
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), _elements};
}

template<typename T>
void Buffer<T>::write(std::span<T const> data, size_t mapped_offset) const {
    auto const charData = std::span<char const>{reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T)};
    BaseBuffer::write(charData, mapped_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::flush(size_t size, size_t offset) const { BaseBuffer::flush(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
void Buffer<T>::invalidate(size_t size, size_t offset) const { BaseBuffer::invalidate(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
void Buffer<T>::copy(CmdBuffer const& cmd_buffer, Buffer const& src, size_t copy_size, size_t src_offset,
    size_t dst_offset) const {
    copy_size = copy_size == constants::WHOLE_SIZE ? constants::WHOLE_SIZE : copy_size * sizeof(T);
    BaseBuffer::copy(cmd_buffer, src, copy_size, src_offset * sizeof(T), dst_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::stage(CmdBuffer const& cmd_buffer, Buffer const& staging_buffer, size_t dst_offset) const {
    BaseBuffer::stage(cmd_buffer, staging_buffer, dst_offset * sizeof(T));
}

template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(size_t size, size_t offset) const {
    if(size == constants::WHOLE_SIZE) return BaseBuffer::descriptorInfo(constants::WHOLE_SIZE, 0);
    return BaseBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}

template<typename T>
void Buffer<T>::write(std::span<T const> data, std::span<T> mapped_memory) {
    auto const charData = std::span<char const>{reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T)};
    auto const charMapped = std::span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    BaseBuffer::write(charData, charMapped);
}

} // namespace cth
