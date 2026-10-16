#pragma once
#include "cvk/base/queue/queue.hpp"
#include "cvk/base/queue/queue_family.hpp"

#include <volk.h>


namespace cvk {
/**
 * @brief queue family and flags of a command pool
 */
struct CmdPoolConfig {
    /**
     * @brief the command buffers of the pool are submitted to queues of this family
     */
    queue_family_index_t queueFamilyIndex = 0;

    /**
     * @brief VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT -> @ref BaseCmdBuffer::reset() is allowed
     */
    VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
        | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    /**
     * @brief config for the family of the queue, default flags
     */
    [[nodiscard]] static constexpr CmdPoolConfig Default(Queue const& queue) {
        return CmdPoolConfig{.queueFamilyIndex = queue.familyIndex()};
    }

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
