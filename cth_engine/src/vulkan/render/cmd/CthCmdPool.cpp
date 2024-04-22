#include "CthCmdPool.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <cth/cth_log.hpp>

#include "CthCmdBuffer.hpp"


namespace cth {

CmdPool::CmdPool(Device* device, const Config& config) : device(device), _config(config) {
    Device::debug_check(device);

    create();
    alloc();
    cth::log::msg("created command pool");
}
CmdPool::~CmdPool() {
    CTH_ERR(primaryBuffers.size() != _config.maxPrimaryBuffers, "all primary cmd buffers must be destroyed prior to the pool")
        throw details->exception();
    CTH_ERR(secondaryBuffers.size() != _config.maxSecondaryBuffers, "all secondary cmd buffers must be destroyed prior to the pool")
        throw details->exception();
    if(_config.maxPrimaryBuffers > 0) vkFreeCommandBuffers(device->get(), vkPool, static_cast<uint32_t>(primaryBuffers.size()), primaryBuffers.data());
    if(_config.maxSecondaryBuffers > 0) vkFreeCommandBuffers(device->get(), vkPool, static_cast<uint32_t>(secondaryBuffers.size()), secondaryBuffers.data());

    vkDestroyCommandPool(device->get(), vkPool, nullptr);
}
void CmdPool::newCmdBuffer(PrimaryCmdBuffer* buffer) {
    CTH_ERR(buffer->pool != this, "cmd buffer must be empty and created with this pool")
        throw details->exception();
    CTH_ERR(primaryBuffers.empty(), "no primary buffers left, this should not happen") throw details->exception();
    buffer->vkBuffer = primaryBuffers.back();
    primaryBuffers.pop_back();

}
void CmdPool::newCmdBuffer(SecondaryCmdBuffer* buffer) {
    CTH_ERR(buffer->pool != this, "cmd buffer must be empty and created with this pool")
        throw details->exception();
    CTH_ERR(secondaryBuffers.empty(), "no secondary buffers left, this should not happen") throw details->exception();

    buffer->vkBuffer = secondaryBuffers.back();
    secondaryBuffers.pop_back();
}
void CmdPool::returnCmdBuffer(PrimaryCmdBuffer* buffer) {
    CTH_ERR(buffer->pool != this, "cmd buffer must be created with this pool") throw details->exception();
    CTH_ERR(primaryBuffers.size() >= _config.maxPrimaryBuffers, "cannot return more than maxPrimaryBuffers") throw details->exception();
    vkResetCommandBuffer(buffer->get(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    primaryBuffers.push_back(buffer->vkBuffer);
    buffer->vkBuffer = VK_NULL_HANDLE;
}
void CmdPool::returnCmdBuffer(SecondaryCmdBuffer* buffer) {
    CTH_ERR(buffer->pool != this, "cmd buffer must be created with this pool") throw details->exception();
    CTH_ERR(secondaryBuffers.size() >= _config.maxSecondaryBuffers, "cannot return more than maxSecondaryBuffers") throw details->exception();

    vkResetCommandBuffer(buffer->get(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    secondaryBuffers.push_back(buffer->vkBuffer);
    buffer->vkBuffer = VK_NULL_HANDLE;
}



void CmdPool::create() {
    const auto info = _config.createInfo();
    const auto result = vkCreateCommandPool(device->get(), &info, nullptr, &vkPool);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create command pool")
        throw cth::except::vk_result_exception{result, details->exception()};
}
void CmdPool::alloc() {
    primaryBuffers.resize(_config.maxPrimaryBuffers);
    secondaryBuffers.resize(_config.maxSecondaryBuffers);


    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkPool;

    if(_config.maxPrimaryBuffers > 0) {
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(primaryBuffers.size());

        auto allocResult = vkAllocateCommandBuffers(device->get(), &allocInfo, primaryBuffers.data());

        CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate {} primary command buffers", _config.maxPrimaryBuffers)
            throw cth::except::vk_result_exception{allocResult, details->exception()};
    }
    if(_config.maxSecondaryBuffers > 0) {
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(secondaryBuffers.size());

        auto allocResult = vkAllocateCommandBuffers(device->get(), &allocInfo, secondaryBuffers.data());

        CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate {} secondary command buffers", _config.maxSecondaryBuffers)
            throw cth::except::vk_result_exception{allocResult, details->exception()};
    }
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
