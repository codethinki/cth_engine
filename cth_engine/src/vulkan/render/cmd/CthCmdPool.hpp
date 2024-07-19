#pragma once


#include <vulkan/vulkan.h>

#include <vector>

#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/utility/CthConstants.hpp"


namespace cth::vk {
class Queue;
class BasicCore;

class CmdBuffer;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;


class CmdPool {
public:
    struct Config;
    CmdPool(const BasicCore* device, const Config& config);
    ~CmdPool();

    /**
    * @throws cth::except::vk_result_exception VkResult of vkCreateCommandPool()
    */
    void create();
    /**
     * @throws cth::except::vk_result_exception VkResult of vkAllocateCommandBuffers() (for primary or secondary buffers)
     */
    void alloc();

    void destroy(DeletionQueue* deletion_queue = nullptr);



    void returnCmdBuffer(PrimaryCmdBuffer* buffer);
    void returnCmdBuffer(SecondaryCmdBuffer* buffer);


    static void destroy(VkDevice vk_device, VkCommandPool vk_pool);

private:
    friend PrimaryCmdBuffer;
    friend SecondaryCmdBuffer;
    void newCmdBuffer(PrimaryCmdBuffer* buffer);
    void newCmdBuffer(SecondaryCmdBuffer* buffer);

    const BasicCore* _core;
    move_ptr<VkCommandPool_T> _handle = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> _primaryBuffers;
    std::vector<VkCommandBuffer> _secondaryBuffers;

public:
    struct Config {
        size_t maxPrimaryBuffers = 0;
        size_t maxSecondaryBuffers = 0;
        uint32_t queueFamilyIndex = constant::QUEUE_FAMILY_IGNORED;
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        static Config Default(const uint32_t queue_family_index, const uint32_t max_primary_buffers, const uint32_t max_secondary_buffers) {
            return Config{max_primary_buffers, max_secondary_buffers, queue_family_index};
        }
        static Config Default(const Queue& queue, uint32_t max_primary_buffers, uint32_t max_secondary_buffers);

    private:
        [[nodiscard]] VkCommandPoolCreateInfo createInfo() const;

        friend CmdPool;
    };

    [[nodiscard]] auto queueFamilyIndex() const { return _config.queueFamilyIndex; }
    [[nodiscard]] auto primaryAvailable() const { return _primaryBuffers.size(); }
    [[nodiscard]] auto secondaryAvailable() const { return _secondaryBuffers.size(); }
    [[nodiscard]] auto primaryCapacity() const { return _config.maxPrimaryBuffers; }
    [[nodiscard]] auto secondaryCapacity() const { return _config.maxSecondaryBuffers; }

    CmdPool(const CmdPool& other) = delete;
    CmdPool& operator=(const CmdPool& other) = delete;
    CmdPool(CmdPool&& other) = default;
    CmdPool& operator=(CmdPool&& other) = default;

private:
    Config _config;

};
}
