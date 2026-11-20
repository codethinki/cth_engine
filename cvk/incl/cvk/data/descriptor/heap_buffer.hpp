#pragma once
#include "cvk/data/buffer/base_buffer.hpp"

#include <volk.h>

#include <cstddef>
#include <string_view>


namespace cvk::dev {
/**
 * buffer config of a descriptor heap
 * @details usage: descriptor heap and device address
 * @details memory: mapped, sequential host writes, prefers device memory
 */
inline constexpr BaseBuffer::Config HEAP_BUFFER_CONFIG{
    .usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    .memory = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    },
};

/**
 * creates the buffer of a descriptor heap
 * @param[out] buffer gets a buffer with `HEAP_BUFFER_CONFIG` and the alignment
 * @param size in bytes
 * @param alignment of the heap address
 * @param maxSize device limit of the heap size
 * @param heap name of the heap for the messages
 * @details memory not device local -> warns: the gpu fetches the descriptors from host memory
 * @pre `Core::created()`
 * @pre `0 < size`
 * @post `buffer.created()`
 * @note calls `BaseBuffer::optDestroy()`
 * @throws cvk::cvk_exception reason: `size > maxSize`
 * @throws cvk::vk_result_exception result of `vmaCreateBuffer()`
 */
void create_heap_buffer(BaseBuffer& buffer, size_t size, size_t alignment, size_t maxSize, std::string_view heap);

/**
 * @return bind info of the whole buffer with the reserved range
 * @pre `buffer.created()`
 */
[[nodiscard]] VkBindHeapInfoEXT heap_bind_info(BaseBuffer const& buffer, size_t reservedOffset, size_t reservedSize);
}
