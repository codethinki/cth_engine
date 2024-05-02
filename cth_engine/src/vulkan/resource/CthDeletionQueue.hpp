#pragma once
#include "interface/CthEngineSettings.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <variant>
#include <vector>

#include "vulkan/utility/CthConstants.hpp"

namespace cth {
class Device;

using std::vector;
using std::variant;
using std::array;

class DeletionQueue {
public:
    using deletable_handle_t = variant<VkDeviceMemory, VkBuffer, VkImage, VkSemaphore>;

    explicit DeletionQueue(Device* device);
    ~DeletionQueue();

    void push(deletable_handle_t handle);
    void clear(uint32_t current_frame);
    void next(const uint32_t next_frame) { _frame = next_frame; }

private:
    static constexpr size_t QUEUES = Constants::MAX_FRAMES_IN_FLIGHT;
    uint32_t _frame = 0;
    Device* _device;
    array<vector<deletable_handle_t>, QUEUES> _queue;

public:
    [[nodiscard]] uint32_t currentFrame() const { return _frame; }

#ifdef _DEBUG
#define  DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) DeletionQueue::debug_check(deletion_queue_ptr)

    static void debug_check(const DeletionQueue* queue);
#else
#define DEBUG_CHECK_DELETION_QUEUE(deletion_queue_ptr) ((void)0)
#endif
};

}
