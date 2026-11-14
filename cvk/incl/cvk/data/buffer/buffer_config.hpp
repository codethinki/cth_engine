#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/data/memory/memory_config.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <algorithm>
#include <vector>


namespace cvk {
/**
 * config of a `BaseBuffer`
 */
struct BufferConfig {
    /**
     * @pre `!= 0`
     */
    VkBufferUsageFlags usage = 0;

    VkBufferCreateFlags flags = 0;

    /**
     * queue families that use the buffer at the same time
     * @details `size < 2` -> `VK_SHARING_MODE_EXCLUSIVE`, else `VK_SHARING_MODE_CONCURRENT`
     * @pre unique
     */
    std::vector<queue_family_index_t> queueFamilies{};

    MemoryConfig memory{}; ///< memory of the buffer, `VMA_MEMORY_USAGE_AUTO` picks the memory type from `usage`

    //TODO review one place for the rule: the create info and the jvk tracking read it
    /**
     * @return `VK_SHARING_MODE_CONCURRENT` for two or more `queueFamilies`, else `VK_SHARING_MODE_EXCLUSIVE`
     */
    [[nodiscard]] constexpr VkSharingMode sharingMode() const {
        return queueFamilies.size() >= 2 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    }

    /**
     * @param size in bytes
     * @attention `pQueueFamilyIndices` points into `queueFamilies`
     */
    [[nodiscard]] constexpr VkBufferCreateInfo createInfo(size_t size) const {
        return VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .size = size,
            .usage = usage,
            .sharingMode = sharingMode(),
            .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilies.size()),
            .pQueueFamilyIndices = queueFamilies.data(),
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
