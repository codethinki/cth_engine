#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <memory>
#include <mutex>

namespace cvk {
struct QueueState {
    /**
     * @brief function table of the device
     * @details stable when the device moves
     */
    not_null_ptr<VolkDeviceTable const> functions;
    vk_not_null<VkQueue> vkQueue;
    /**
     * @brief vulkan family index
     */
    uint32_t familyIndex;
    /**
     * @brief index in the family
     */
    uint32_t queueIndex;

    /**
     * @brief guards the vulkan queue, shared by every copy of the queue
     * @pre not nullptr
     */
    std::shared_ptr<std::mutex> vkQueueMtx = std::make_shared<std::mutex>();


    static constexpr void debug_check(QueueState const& state) {
        CTH_CRITICAL(state.vkQueueMtx == nullptr, "mutex must not be nullptr") {}
    }
};
}
