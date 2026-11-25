#pragma once
#include "cvk/data/buffer/base_buffer.hpp"

#include <volk.h>

#include <cstddef>
#include <string_view>


namespace cvk::dev {
inline constexpr BaseBuffer::Config HEAP_BUFFER_CONFIG{
    .usage = VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    .memory = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    },
};

void create_heap_buffer(
    BaseBuffer& buffer,
    size_t size,
    size_t alignment,
    size_t maxSize,
    std::string_view heap
);

[[nodiscard]] VkBindHeapInfoEXT heap_bind_info(
    BaseBuffer const& buffer,
    size_t reservedOffset,
    size_t reservedSize
);
}
