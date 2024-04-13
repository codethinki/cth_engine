#pragma once


#include <queue>
#include <vulkan/vulkan.h>

#include <vector>

namespace cth {
using namespace std;
class Device;
class CmdBuffer;

class CmdPool {
public:
    struct Config;
    CmdPool(Device* device, const Config& config);
    ~CmdPool();
private:
    void create(const Config& config);
    void alloc(const Config& config);

    Device* device;
    VkCommandPool vkPool = VK_NULL_HANDLE;
    vector<VkCommandBuffer> primaryBuffers{};
    queue<uint32_t> unusedPrimaries{};
    vector<VkCommandBuffer> secondaryBuffers{};
    queue<uint32_t> unusedSecondaries{};
};
}

//Config

namespace cth {
struct CmdPool::Config {
    size_t maxPrimaryBuffers = 0;
    size_t maxSecondaryBuffers = 0;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    static Config Default(const uint32_t queue_family_index, const uint32_t max_primary_buffers, const uint32_t max_secondary_buffers) {
        return Config{max_primary_buffers, max_secondary_buffers, queue_family_index};
    }

private:
    VkCommandPoolCreateInfo createInfo() const;

    friend CmdPool;
};
}
