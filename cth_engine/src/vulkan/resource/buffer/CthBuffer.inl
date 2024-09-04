#pragma once
#include "../CthDestructionQueue.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/memory/CthMemory.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {

template<typename T>
Buffer<T>::Buffer(cth::not_null<BasicCore const*> core, size_t  element_count, VkBufferUsageFlags  usage_flags,
    VkMemoryPropertyFlags memory_property_flags) : BasicBuffer{core, element_count * sizeof(T), usage_flags},
    _elements{element_count}, _destructionQueue{_core->destructionQueue()} {
    Memory* memory = new Memory{core, memory_property_flags};

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
void Buffer<T>::destroy(DestructionQueue* destruction_queue) {
    if(destruction_queue)_destructionQueue = destruction_queue;
    if(_state.memory->created()) _state.memory->destroy();
    BasicBuffer::destroy(_destructionQueue);
}

template<typename T>
std::span<T> Buffer<T>::map(size_t  size, size_t offset) {
    auto const charSpan = BasicBuffer::map(size * sizeof(T), offset * sizeof(T));
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), size};
}
template<typename T>
std::span<T> Buffer<T>::map() {
    auto const charSpan = BasicBuffer::map();
    return std::span<T>{reinterpret_cast<T*>(charSpan.data()), _elements};
}

template<typename T>
void Buffer<T>::stage(CmdBuffer const& cmd_buffer, Buffer<T> const& staging_buffer, size_t  dst_offset) const {
    BasicBuffer::stage(cmd_buffer, staging_buffer, dst_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::write(std::span<T const> data, std::span<T> mapped_memory) {
    auto const charData = std::span<char const>{reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T)};
    auto const charMapped = std::span<char>{reinterpret_cast<char*>(mapped_memory.data()), mapped_memory.size() * sizeof(T)};
    BasicBuffer::write(charData, charMapped);
}
template<typename T>
void Buffer<T>::write(std::span<T const> data, size_t  mapped_offset) const {
    auto const charData = std::span<char const>{reinterpret_cast<char const*>(data.data()), data.size() * sizeof(T)};
    BasicBuffer::write(charData, mapped_offset * sizeof(T));
}

template<typename T>
void Buffer<T>::copy(CmdBuffer const& cmd_buffer, Buffer<T> const& src, size_t  copy_size, size_t src_offset,
    size_t dst_offset) const {
    copy_size = copy_size == constants::WHOLE_SIZE ? constants::WHOLE_SIZE : copy_size * sizeof(T);
    BasicBuffer::copy(cmd_buffer, src, copy_size, src_offset * sizeof(T), dst_offset * sizeof(T));
}

template<typename T>
VkResult Buffer<T>::flush(size_t  size, size_t offset) const { return BasicBuffer::flush(size * sizeof(T), offset * sizeof(T)); }

template<typename T>
VkResult Buffer<T>::invalidate(size_t  size, size_t offset) const { return BasicBuffer::invalidate(size * sizeof(T), offset * sizeof(T)); }


template<typename T>
VkDescriptorBufferInfo Buffer<T>::descriptorInfo(size_t  size, size_t offset) const {
    if(size == constants::WHOLE_SIZE) return BasicBuffer::descriptorInfo(constants::WHOLE_SIZE, 0);
    return BasicBuffer::descriptorInfo(size * sizeof(T), offset * sizeof(T));
}

template<typename T>
void Buffer<T>::setMemory(Memory* new_memory) {
    CTH_ERR(new_memory != nullptr && new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();
    if(_state.memory) destroyMemory();
    BasicBuffer::setMemory(new_memory);
}

} // namespace cth
