#include "CthCmdPool.hpp"

#include "CthCmdBuffer.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthQueue.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"



namespace cth::vk {

CmdPool::CmdPool(cth::not_null<Core const*> device, Config const& config) : _core(device), _config(config) {
    DEBUG_CHECK_CORE(device);

    create();
    alloc();
    cth::log::msg("created command pool");
}
CmdPool::~CmdPool() { destroy(); }

void CmdPool::destroy(DestructionQueue* destruction_queue) {
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue);
    CTH_ERR(_primaryBuffers.size() != _config.maxPrimaryBuffers, "all primary cmd buffers must be destroyed prior to the pool")
        throw details->exception();
    CTH_ERR(_secondaryBuffers.size() != _config.maxSecondaryBuffers, "all secondary cmd buffers must be destroyed prior to the pool")
        throw details->exception();

    if(destruction_queue) {
        for(auto& primary : _primaryBuffers) destruction_queue->push(primary, _handle.get());
        for(auto& secondary : _secondaryBuffers) destruction_queue->push(secondary, _handle.get());
        destruction_queue->push(_handle.get());

        return;
    }
    if(_config.maxPrimaryBuffers > 0) CmdBuffer::free(_core->vkDevice(), _handle.get(), _primaryBuffers);
    if(_config.maxSecondaryBuffers > 0) CmdBuffer::free(_core->vkDevice(), _handle.get(), _secondaryBuffers);

    destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;
}

void CmdPool::newCmdBuffer(PrimaryCmdBuffer* buffer) {
    CTH_ERR(buffer->_pool.get() != this, "cmd buffer must be empty and created with this pool")
        throw details->exception();
    CTH_ERR(_primaryBuffers.empty(), "no primary buffers left, this should not happen") throw details->exception();
    buffer->_handle = _primaryBuffers.back();
    _primaryBuffers.pop_back();

}
void CmdPool::newCmdBuffer(SecondaryCmdBuffer* buffer) {
    CTH_ERR(buffer->_pool.get() != this, "cmd buffer must be empty and created with this pool")
        throw details->exception();
    CTH_ERR(_secondaryBuffers.empty(), "no secondary buffers left, this should not happen") throw details->exception();

    buffer->_handle = _secondaryBuffers.back();
    _secondaryBuffers.pop_back();
}
void CmdPool::returnCmdBuffer(PrimaryCmdBuffer* buffer) {
    CTH_ERR(buffer->_pool.get() != this, "cmd buffer must be created with this pool") throw details->exception();
    CTH_ERR(_primaryBuffers.size() >= _config.maxPrimaryBuffers, "cannot return more than maxPrimaryBuffers") throw details->exception();
    vkResetCommandBuffer(buffer->get(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    _primaryBuffers.push_back(buffer->get());
    buffer->_handle = VK_NULL_HANDLE;
}
void CmdPool::returnCmdBuffer(SecondaryCmdBuffer* buffer) {
    CTH_ERR(buffer->_pool.get() != this, "cmd buffer must be created with this pool") throw details->exception();
    CTH_ERR(_secondaryBuffers.size() >= _config.maxSecondaryBuffers, "cannot return more than maxSecondaryBuffers") throw details->exception();

    vkResetCommandBuffer(buffer->get(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    _secondaryBuffers.push_back(buffer->get());
    buffer->_handle = VK_NULL_HANDLE;
}
void CmdPool::destroy(VkDevice vk_device, VkCommandPool vk_pool) {
    CTH_WARN(vk_pool == VK_NULL_HANDLE, "pool should not be invalid (VK_NULL_HANDLE)") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroyCommandPool(vk_device, vk_pool, nullptr);
}



void CmdPool::create() {
    auto const info = _config.createInfo();

    VkCommandPool ptr = VK_NULL_HANDLE;
    auto const result = vkCreateCommandPool(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create command pool")
        throw cth::vk::result_exception{result, details->exception()};

    _handle = ptr;
}
void CmdPool::alloc() {
    _primaryBuffers.resize(_config.maxPrimaryBuffers);
    _secondaryBuffers.resize(_config.maxSecondaryBuffers);


    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _handle.get();

    if(_config.maxPrimaryBuffers > 0) {
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(_primaryBuffers.size());

        auto const allocResult = vkAllocateCommandBuffers(_core->vkDevice(), &allocInfo, _primaryBuffers.data());

        CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate {} primary command buffers", _config.maxPrimaryBuffers)
            throw cth::vk::result_exception{allocResult, details->exception()};
    }
    if(_config.maxSecondaryBuffers > 0) {
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(_secondaryBuffers.size());

        auto const allocResult = vkAllocateCommandBuffers(_core->vkDevice(), &allocInfo, _secondaryBuffers.data());

        CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate {} secondary command buffers", _config.maxSecondaryBuffers)
            throw cth::vk::result_exception{allocResult, details->exception()};
    }
}



}

//Config

namespace cth::vk {
CmdPool::Config CmdPool::Config::Default(Queue const& queue, uint32_t  max_primary_buffers, uint32_t  max_secondary_buffers) {
    return Config{max_primary_buffers, max_secondary_buffers, queue.familyIndex()};
}


VkCommandPoolCreateInfo CmdPool::Config::createInfo() const {
    return VkCommandPoolCreateInfo{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        flags,
        queueFamilyIndex
    };
}
}
