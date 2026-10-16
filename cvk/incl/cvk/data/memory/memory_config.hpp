#pragma once
#include <volk.h>
#include <vk_mem_alloc.h>


namespace cvk {
/**
 * @brief what the memory is for, VMA picks the memory type from it
 */
struct MemoryConfig {
    /**
     * @brief VMA_ALLOCATION_CREATE_* flags, e.g. host access, mapped, dedicated memory
     */
    VmaAllocationCreateFlags flags = 0;

    /**
     * @brief VMA_MEMORY_USAGE_AUTO* -> only for buffers and images that VMA creates with the memory
     */
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_AUTO;

    /**
     * @brief the memory type must have these flags
     */
    VkMemoryPropertyFlags requiredFlags = 0;

    /**
     * @brief VMA prefers memory types with these flags
     */
    VkMemoryPropertyFlags preferredFlags = 0;

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
