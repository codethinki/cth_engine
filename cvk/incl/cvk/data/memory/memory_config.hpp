#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>


namespace cvk {
/**
 * config of a @ref Memory
 */
struct MemoryConfig {
    /**
     * VMA_ALLOCATION_CREATE_* flags
     */
    VmaAllocationCreateFlags flags = 0;

    /**
     * VMA_MEMORY_USAGE_AUTO* -> only for buffers and images that VMA creates with the memory
     */
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO;

    /**
     * the memory type must have these flags
     */
    VkMemoryPropertyFlags requiredFlags = 0;

    /**
     * VMA prefers memory types with these flags
     */
    VkMemoryPropertyFlags preferredFlags = 0;

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
            .minAlignment = 0,
        };
    }

    /**
     * @return @ref usage is VMA_MEMORY_USAGE_AUTO*
     */
    [[nodiscard]] constexpr bool autoUsage() const {
        return usage == VMA_MEMORY_USAGE_AUTO || usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE ||
            usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    }
};
}
