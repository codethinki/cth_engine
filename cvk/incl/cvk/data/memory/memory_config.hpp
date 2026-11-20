#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>


namespace cvk {
/**
 * config of a `Memory`
 */
struct MemoryConfig {
    VmaAllocationCreateFlags flags = 0; ///< `VMA_ALLOCATION_CREATE_*` flags

    /**
     * `VMA_MEMORY_USAGE_AUTO*` -> only for buffers and images that VMA creates with the memory
     */
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO;

    VkMemoryPropertyFlags requiredFlags = 0; ///< the memory type must have these flags

    VkMemoryPropertyFlags preferredFlags = 0; ///< VMA prefers memory types with these flags

    /**
     * minimum alignment of the allocation, `0` -> the alignment of the memory requirements
     * @pre `0` or a power of two
     */
    VkDeviceSize minAlignment = 0;

    /**
     * @return create info with the members of the config, no pool
     */
    [[nodiscard]] constexpr VmaAllocationCreateInfo createInfo() const {
        return VmaAllocationCreateInfo{
            .flags = flags,
            .usage = usage,
            .requiredFlags = requiredFlags,
            .preferredFlags = preferredFlags,
            .memoryTypeBits = 0,
            .pool = VK_NULL_HANDLE,
            .pUserData = nullptr,
            .priority = 0.0f,
            .minAlignment = minAlignment,
        };
    }

    /**
     * @return `usage` is `VMA_MEMORY_USAGE_AUTO*`
     */
    [[nodiscard]] constexpr bool autoUsage() const {
        return usage == VMA_MEMORY_USAGE_AUTO || usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE ||
            usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    }
};
}
