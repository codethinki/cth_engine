#pragma once
#include "jvk/utility/types.hpp"

#include <mutex>

namespace jvk {
class Device;

struct QueueState {
    not_null<Device const*> device;
    vk_not_null<VkQueue> vkQueue;
    /**
     * vk family index
     */
    uint32_t familyIndex;
    /**
     * index in the family
     */
    uint32_t queueIndex;

    /**
     * @pre not `nullptr` 
     */
    std::shared_ptr<std::mutex> vkQueueMtx = std::make_shared<std::mutex>();


    static void debug_check(QueueState const& state) {
        CTH_CRITICAL(state.vkQueueMtx == nullptr, "mutex must not be nullptr") {}
    }
};
}
