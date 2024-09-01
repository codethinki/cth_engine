#pragma once


#include <vulkan/vulkan.h>

#include <vector>

#include "vulkan/resource/buffer/CthBasicBuffer.hpp"
#include "vulkan/utility/cth_constants.hpp"


namespace cth::vk {
class Queue;
class BasicCore;

class CmdBuffer;
class PrimaryCmdBuffer;
class SecondaryCmdBuffer;


class CmdPool {
public:
    struct Config;
    CmdPool(not_null<BasicCore const*> device, Config const& config);
    ~CmdPool();

    /**
    * @throws cth::except::vk_result_exception VkResult of vkCreateCommandPool()
    */
    void create();
    /**
     * @throws cth::except::vk_result_exception VkResult of vkAllocateCommandBuffers() (for primary or secondary buffers)
     */
    void alloc();

    void destroy(DestructionQueue* destruction_queue = nullptr);



    void returnCmdBuffer(PrimaryCmdBuffer* buffer);
    void returnCmdBuffer(SecondaryCmdBuffer* buffer);


    static void destroy(VkDevice vk_device, VkCommandPool vk_pool);

private:
    friend PrimaryCmdBuffer;
    friend SecondaryCmdBuffer;
    void newCmdBuffer(PrimaryCmdBuffer* buffer);
    void newCmdBuffer(SecondaryCmdBuffer* buffer);

    not_null<BasicCore const*> _core;
    move_ptr<VkCommandPool_T> _handle = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> _primaryBuffers;
    std::vector<VkCommandBuffer> _secondaryBuffers;

public:
    struct Config {
        size_t maxPrimaryBuffers = 0;
        size_t maxSecondaryBuffers = 0;
        uint32_t queueFamilyIndex = constants::QUEUE_FAMILY_IGNORED;
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        static Config Default(uint32_t  queue_family_index, uint32_t  max_primary_buffers, uint32_t  max_secondary_buffers) {
            return Config{max_primary_buffers, max_secondary_buffers, queue_family_index};
        }
        static Config Default(Queue const& queue, uint32_t max_primary_buffers, uint32_t max_secondary_buffers);

    private:
        [[nodiscard]] VkCommandPoolCreateInfo createInfo() const;

        friend CmdPool;
    };

    [[nodiscard]] auto queueFamilyIndex() const { return _config.queueFamilyIndex; }
    [[nodiscard]] auto primaryAvailable() const { return _primaryBuffers.size(); }
    [[nodiscard]] auto secondaryAvailable() const { return _secondaryBuffers.size(); }
    [[nodiscard]] auto primaryCapacity() const { return _config.maxPrimaryBuffers; }
    [[nodiscard]] auto secondaryCapacity() const { return _config.maxSecondaryBuffers; }

    CmdPool(CmdPool const& other) = delete;
    CmdPool& operator=(CmdPool const& other) = delete;
    CmdPool(CmdPool&& other) = default;
    CmdPool& operator=(CmdPool&& other) = default;

private:
    Config _config;

};
}
