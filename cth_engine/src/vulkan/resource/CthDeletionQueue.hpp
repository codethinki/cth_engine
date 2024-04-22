#pragma once
#include "interface/CthEngineSettings.hpp"

#include <vulkan/vulkan.h>

#include <array>
#include <variant>
#include <vector>

namespace cth {
class Device;

using std::vector;
using std::variant;
using std::array;

class DeletionQueue {
public:
    using deletable_handle_t = variant<VkDeviceMemory, VkBuffer, VkImage>;

    explicit DeletionQueue(Device* device);
    ~DeletionQueue();

    void enqueue(deletable_handle_t handle);
    void clear(uint32_t current_frame);
    void next(const uint32_t next_frame) { frame = next_frame; }

private:
    uint32_t frame = 0;
    Device* device;
    array<vector<deletable_handle_t>, MAX_FRAMES_IN_FLIGHT + 1> _queue;

public:
    [[nodiscard]] uint32_t currentFrame() const { return frame; }

#ifdef _DEBUG
    static void debug_check(const DeletionQueue* queue);
#else
    static void debug_check(const DeletionQueue* queue) {};
#endif
};

}
