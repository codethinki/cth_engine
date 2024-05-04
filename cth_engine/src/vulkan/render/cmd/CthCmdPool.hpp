#pragma once


#include <vulkan/vulkan.h>

#include <vector>

#include "vulkan/utility/CthConstants.hpp"

namespace cth {
using namespace std;
class Device;

class CmdBuffer;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;


class CmdPool {
public:
    struct Config;
    CmdPool(Device* device, const Config& config);
    ~CmdPool();


    void newCmdBuffer(PrimaryCmdBuffer* buffer);
    void newCmdBuffer(SecondaryCmdBuffer* buffer);

    void returnCmdBuffer(PrimaryCmdBuffer* buffer);
    void returnCmdBuffer(SecondaryCmdBuffer* buffer);

private:
    /**
     * \throws cth::except::vk_result_exception VkResult of vkCreateCommandPool()
     */
    void create();
    /**
     * \throws cth::except::vk_result_exception VkResult of vkAllocateCommandBuffers() (for primary or secondary buffers)
     */
    void alloc();

    Device* device;
    VkCommandPool vkPool = VK_NULL_HANDLE;
    vector<VkCommandBuffer> primaryBuffers;
    vector<VkCommandBuffer> secondaryBuffers;

public:
    struct CmdPool::Config {
        size_t maxPrimaryBuffers = 0;
        size_t maxSecondaryBuffers = 0;
        uint32_t queueFamilyIndex = Constant::QUEUE_FAMILY_IGNORED;
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        static Config Default(const uint32_t queue_family_index, const uint32_t max_primary_buffers, const uint32_t max_secondary_buffers) {
            return Config{max_primary_buffers, max_secondary_buffers, queue_family_index};
        }

    private:
        [[nodiscard]] VkCommandPoolCreateInfo createInfo() const;

        friend CmdPool;
    };

    [[nodiscard]] auto queueFamilyIndex() const { return config_.queueFamilyIndex; }
    [[nodiscard]] auto primaryAvailable() const { return primaryBuffers.size(); }
    [[nodiscard]] auto secondaryAvailable() const { return secondaryBuffers.size(); }
    [[nodiscard]] auto primaryCapacity() const { return config_.maxPrimaryBuffers; }
    [[nodiscard]] auto secondaryCapacity() const { return config_.maxSecondaryBuffers; }

    CmdPool(const CmdPool& other) = delete;
    CmdPool(CmdPool&& other) = delete;
    CmdPool& operator=(const CmdPool& other) = delete;
    CmdPool& operator=(CmdPool&& other) = delete;
private:
    Config config_;

};
}
