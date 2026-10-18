#pragma once
#include "cvk/base/queue/queue.hpp"
#include "cvk/base/queue/queue_family.hpp"

#include <volk.h>


namespace cvk {
/**
 * @brief queue family and flags of a command pool
 */
struct CmdPoolConfig {
    static constexpr VkCommandPoolCreateFlags DEFAULT_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    constexpr CmdPoolConfig() = default;

    explicit constexpr CmdPoolConfig(
        queue_family_index_t queueFamilyIndex,
        VkCommandPoolCreateFlags flags = DEFAULT_FLAGS
    ) : queueFamilyIndex{queueFamilyIndex}, flags{flags} {}

    /**
     * @brief config for the family of the queue
     */
    explicit constexpr CmdPoolConfig(Queue const& queue, VkCommandPoolCreateFlags flags = DEFAULT_FLAGS) :
        CmdPoolConfig{queue.familyIndex(), flags} {}

    /**
     * @brief the command buffers of the pool are submitted to queues of this family
     */
    queue_family_index_t queueFamilyIndex = 0;

    /**
     * @brief VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT -> @ref BaseCmdBuffer::reset() is allowed
     */
    VkCommandPoolCreateFlags flags = DEFAULT_FLAGS;

    [[nodiscard]] constexpr VkCommandPoolCreateInfo createInfo() const {
        return VkCommandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags,
            .queueFamilyIndex = queueFamilyIndex,
        };
    }
};
}
