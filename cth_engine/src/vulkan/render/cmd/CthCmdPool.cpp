#include "CthCmdPool.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <cth/cth_log.hpp>


namespace cth {

CmdPool::CmdPool(Device* device, const Config& config) : device(device) {
    create(config);
    alloc(config);
}
CmdPool::~CmdPool() {
    if(!primaryBuffers.empty()) 
        vkFreeCommandBuffers(device->get(), vkPool, static_cast<uint32_t>(primaryBuffers.size()), primaryBuffers.data());

    if(!secondaryBuffers.empty()) 
        vkFreeCommandBuffers(device->get(), vkPool, static_cast<uint32_t>(secondaryBuffers.size()), secondaryBuffers.data());

    vkDestroyCommandPool(device->get(), vkPool, nullptr);
}


void CmdPool::create(const Config& config) {
    const auto info = config.createInfo();
    const auto result = vkCreateCommandPool(device->get(), &info, nullptr, &vkPool);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create command pool")
        throw cth::except::vk_result_exception{result, details->exception()};
}
void CmdPool::alloc(const Config& config) {
    primaryBuffers.resize(config.maxPrimaryBuffers);
    secondaryBuffers.resize(config.maxSecondaryBuffers);


    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = vkPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(config.maxPrimaryBuffers);

    vkAllocateCommandBuffers(device->get(), &allocInfo, primaryBuffers.data());

    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(config.maxSecondaryBuffers);
    vkAllocateCommandBuffers(device->get(), &allocInfo, primaryBuffers.data());

    for(auto i = 0u; i < primaryBuffers.size(); ++i) unusedPrimaries.push(i);
    for(auto i = 0u; i < secondaryBuffers.size(); ++i) unusedSecondaries.push(i);
    //TEMP complete this 
}


}

//Config

namespace cth {
VkCommandPoolCreateInfo CmdPool::Config::createInfo() const {
    return VkCommandPoolCreateInfo{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        flags,
        queueFamilyIndex
    };
}
}
