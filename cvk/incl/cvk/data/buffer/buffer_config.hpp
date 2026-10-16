#pragma once
#include "cvk/data/memory/memory_config.hpp"

#include <volk.h>


namespace cvk {
/**
 * @brief what the buffer is for and where its memory lives
 */
struct BufferConfig {
    /**
     * @pre != 0
     */
    VkBufferUsageFlags usage = 0;

    /**
     * @brief memory of the buffer, VMA_MEMORY_USAGE_AUTO picks the memory type from @ref usage
     */
    MemoryConfig memory{};

    [[nodiscard]] constexpr VkBufferCreateInfo createInfo(size_t size) const {
        return VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };
    }
};
}
