#pragma once
#include "../CthDeletionQueue.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/memory/CthMemory.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {

template<typename T>
Buffer<T>::Buffer(BasicCore const* core, DeletionQueue* deletion_queue, size_t const element_count, VkBufferUsageFlags const usage_flags,
    VkMemoryPropertyFlags const memory_property_flags) : BasicBuffer(core, element_count * sizeof(T), usage_flags),
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
void Buffer<T>::wrap(VkBuffer vk_buffer, State const& state) {
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
std::span<T> Buffer<T>::map(size_t const size, size_t const offset) {
    auto const charSpan = BasicBuffer::map(size * sizeof(T), offset * sizeof(T));
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T>
std::span<T> Buffer<T>::map() {
    auto const charSpan = BasicBuffer::map();
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), _elements};
}

template<typename T>
void Buffer<T>::stage(CmdBuffer const& cmd_buffer, Buffer<T> const& staging_buffer, size_t const dst_offset) const {
    BasicBuffer::stage(cmd_buffer, staging_buffer, dst_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::write(std::span<T const> data, std::span<T> mapped_memory) {
    auto const charData = std::span<char const>{reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T)};
    auto const charMapped = std::span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    BasicBuffer::write(charData, charMapped);
}
template<typename T>
void Buffer<T>::write(std::span<T const> data, size_t const mapped_offset) const {
    auto const charData = std::span<char const>{reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T)};
    BasicBuffer::write(charData, mapped_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::copy(CmdBuffer const& cmd_buffer, Buffer<T> const& src, size_t const copy_size, size_t const src_offset,
    size_t const dst_offset) const {
    copy_size = copy_size == constants::WHOLE_SIZE ? constants::WHOLE_SIZE : copy_size * sizeof(T);
    BasicBuffer::copy(cmd_buffer, src, copy_size, src_offset * sizeof(T), dst_offset * sizeof(T));
}

template<typename T>
VkResult Buffer<T>::flush(size_t const size, size_t const offset) const { return BasicBuffer::flush(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
VkResult Buffer<T>::invalidate(size_t const size, size_t const offset) const { return BasicBuffer::invalidate(size * sizeof(T), offset * sizeof(T)); }


template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(size_t const size, size_t const offset) const {
    if(size == constants::WHOLE_SIZE) return BasicBuffer::descriptorInfo(constants::WHOLE_SIZE, 0);
    return BasicBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}

template<typename T>
void Buffer<T>::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory != nullptr && new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();
    if(_state.memory) destroyMemory();
    BasicBuffer::setMemory(new_memory);
}

} // namespace cth
