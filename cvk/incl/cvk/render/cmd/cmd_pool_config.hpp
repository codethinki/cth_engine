#pragma once
#include "cvk/base/queue/queue_family.hpp"

#include <volk.h>


namespace cvk {
/**
 * @brief queue family and flags of a command pool
 */
struct CmdPoolConfig {
    static constexpr VkCommandPoolCreateFlags DEFAULT_FLAGS = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    /**
     * @brief the family of the queues that the command buffers are submitted to
     * @details VK_QUEUE_FAMILY_IGNORED is not allowed at create
     */
    queue_family_index_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

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
