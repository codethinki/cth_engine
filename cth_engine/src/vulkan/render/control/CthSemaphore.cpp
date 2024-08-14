#include "CthSemaphore.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {


BasicSemaphore::BasicSemaphore(BasicCore const* core) : _core(core) {}

void BasicSemaphore::create() { createHandle(createInfo()); }

void BasicSemaphore::destroy(DestructionQueue* destruction_queue) {
    if(destruction_queue) destruction_queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;
}


void BasicSemaphore::destroy(VkDevice vk_device, VkSemaphore vk_semaphore) {
    CTH_WARN(vk_semaphore == VK_NULL_HANDLE, "vk_semaphore invalid") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroySemaphore(vk_device, vk_semaphore, nullptr);
}

VkSemaphoreCreateInfo BasicSemaphore::createInfo() {
    constexpr VkSemaphoreCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    return info;
}
void BasicSemaphore::createHandle(VkSemaphoreCreateInfo const& info) {
    CTH_WARN(_handle != VK_NULL_HANDLE, "semaphore handle replaced (potential memory leak)") {}


    VkSemaphore ptr = VK_NULL_HANDLE;
    auto const createResult = vkCreateSemaphore(_core->vkDevice(), &info, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create semaphore")
        throw cth::except::vk_result_exception{createResult,
            details->exception()};
    _handle = ptr;
}



#ifdef _DEBUG

void BasicSemaphore::debug_check(BasicSemaphore const* semaphore) {
    CTH_ERR(semaphore == nullptr, "semaphore must not be nullptr") throw details->exception();
    CTH_ERR(semaphore->_handle == nullptr, "semaphore must be a valid handle") throw details->exception();
}
void BasicSemaphore::debug_check_leak(BasicSemaphore const* semaphore) {
    CTH_WARN(semaphore->_handle != nullptr, "semaphore handle replaced (potential memory leak)") {}
}

#endif

} //namespace cth


namespace cth::vk {
Semaphore::Semaphore(BasicCore const* core, bool const create) : BasicSemaphore(core) {
    if(create) BasicSemaphore::create();
}
Semaphore::~Semaphore() { if(get() != VK_NULL_HANDLE) BasicSemaphore::destroy(_core->destructionQueue()); }

void Semaphore::create() {
    if(get() != VK_NULL_HANDLE) BasicSemaphore::destroy(_core->destructionQueue());

    BasicSemaphore::create();
}
void Semaphore::destroy(DestructionQueue* destruction_queue) {
    if(destruction_queue) BasicSemaphore::destroy(destruction_queue);
    else BasicSemaphore::destroy(_core->destructionQueue());
}
}
