#pragma once
#include "../CthDeletionQueue.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/memory/CthMemory.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


namespace cth::vk {

template<typename T>
Buffer<T>::Buffer(const BasicCore* core, DeletionQueue* deletion_queue, const size_t element_count, const VkBufferUsageFlags usage_flags,
    const VkMemoryPropertyFlags memory_property_flags) : BasicBuffer(core, element_count * sizeof(T), usage_flags),
    _elements(element_count), _deletionQueue(deletion_queue) {
    BasicMemory* memory = new Memory{core, deletion_queue, memory_property_flags};

    BasicBuffer::create();
    BasicBuffer::alloc(memory);
    BasicBuffer::bind();
}
template<typename T>
Buffer<T>::~Buffer() {
    Buffer<T>::destroy();
    if(_state.memory) destroyMemory();
}
template<typename T>
void Buffer<T>::wrap(VkBuffer vk_buffer, const State& state) {
    if(get() != VK_NULL_HANDLE) destroy();
    if(state.memory) destroyMemory();


    BasicBuffer::wrap(vk_buffer, state);
}

template<typename T>
void Buffer<T>::create() {
    if(get() != VK_NULL_HANDLE) destroy();
    BasicBuffer::create();
}

template<typename T>
void Buffer<T>::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue)_deletionQueue = deletion_queue;
    if(_state.memory->allocated()) _state.memory->free(deletion_queue);
    BasicBuffer::destroy(_deletionQueue);
}

template<typename T>
std::span<T> Buffer<T>::map(const size_t size, const size_t offset) {
    const auto charSpan = BasicBuffer::map(size * sizeof(T), offset * sizeof(T));
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T>
std::span<T> Buffer<T>::map() {
    const auto charSpan = BasicBuffer::map();
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), _elements};
}

template<typename T>
void Buffer<T>::stage(const CmdBuffer& cmd_buffer, const Buffer<T>& staging_buffer, const size_t dst_offset) const {
    BasicBuffer::stage(cmd_buffer, staging_buffer, dst_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::write(std::span<const T> data, std::span<T> mapped_memory) {
    const auto charData = std::span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    const auto charMapped = std::span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    BasicBuffer::write(charData, charMapped);
}
template<typename T>
void Buffer<T>::write(std::span<const T> data, const size_t mapped_offset) const {
    const auto charData = std::span<const char>{reinterpret_cast<const char*>(data.data()), data.size() * sizeof(T)};
    BasicBuffer::write(charData, mapped_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::copy(const CmdBuffer& cmd_buffer, const Buffer<T>& src, const size_t copy_size, const size_t src_offset,
    const size_t dst_offset) const {
    copy_size = copy_size == constant::WHOLE_SIZE ? constant::WHOLE_SIZE : copy_size * sizeof(T);
    BasicBuffer::copy(cmd_buffer, src, copy_size, src_offset * sizeof(T), dst_offset * sizeof(T));
}

template<typename T>
VkResult Buffer<T>::flush(const size_t size, const size_t offset) const { return BasicBuffer::flush(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
VkResult Buffer<T>::invalidate(const size_t size, const size_t offset) const { return BasicBuffer::invalidate(size * sizeof(T), offset * sizeof(T)); }


template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(const size_t size, const size_t offset) const {
    if(size == constant::WHOLE_SIZE) return BasicBuffer::descriptorInfo(constant::WHOLE_SIZE, 0);
    return BasicBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}

template<typename T>
void Buffer<T>::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory != nullptr && new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();
    if(_state.memory) destroyMemory();
    BasicBuffer::setMemory(new_memory);
}

} // namespace cth
