#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/memory/memory_config.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <vector>


namespace cvk {
/**
 * @brief what the buffer is for, who uses it and where its memory lives
 */
struct BufferConfig {
    /**
     * @pre != 0
     */
    VkBufferUsageFlags usage = 0;

    /**
     * @brief sparse binding, protected memory, ...
     */
    VkBufferCreateFlags flags = 0;

    /**
     * @brief queue families that use the buffer at the same time
     * @details size < 2 -> VK_SHARING_MODE_EXCLUSIVE, else VK_SHARING_MODE_CONCURRENT
     * @pre unique
     */
    std::vector<queue_family_index_t> queueFamilies{};

    /**
     * @brief memory of the buffer, VMA_MEMORY_USAGE_AUTO picks the memory type from @ref usage
     */
    MemoryConfig memory{};

    /**
     * @param size in bytes
     * @attention pQueueFamilyIndices points into @ref queueFamilies
     */
    [[nodiscard]] constexpr VkBufferCreateInfo createInfo(size_t size) const {
        bool const concurrent = queueFamilies.size() >= 2;

        return VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .size = size,
            .usage = usage,
            .sharingMode = concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = concurrent ? static_cast<uint32_t>(queueFamilies.size()) : 0,
            .pQueueFamilyIndices = concurrent ? queueFamilies.data() : nullptr,
        };
    }

    static constexpr void debug_check(BufferConfig const&);
};
}

namespace cvk {
constexpr void BufferConfig::debug_check(BufferConfig const& config) {
    CTH_CRITICAL(config.usage == 0, "usage must not be 0") {}

    auto families = config.queueFamilies;
    std::ranges::sort(families);
    CTH_CRITICAL(std::ranges::adjacent_find(families) != families.end(), "queue families must be unique") {}
}
}
