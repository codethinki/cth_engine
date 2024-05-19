#include "CthSemaphore.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDeletionQueue.hpp"
#include "vulkan/utility/CthVkUtils.hpp"

#include <cth/cth_log.hpp>

#include "vulkan/base/CthCore.hpp"


namespace cth {


BasicSemaphore::BasicSemaphore(const BasicCore* core) : _core(core) {}

void BasicSemaphore::create() {
    CTH_WARN(_handle != VK_NULL_HANDLE, "semaphore handle replaced (potential memory leak)");

    const auto& info = createInfo();

    VkSemaphore ptr = VK_NULL_HANDLE;
    const auto createResult = vkCreateSemaphore(_core->vkDevice(), &info, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create semaphore")
        throw cth::except::vk_result_exception{createResult,
            details->exception()};
    _handle = ptr;
}
void BasicSemaphore::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) deletion_queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;
}


void BasicSemaphore::destroy(VkDevice vk_device, VkSemaphore vk_semaphore) {
    CTH_WARN(vk_semaphore == VK_NULL_HANDLE, "vk_semaphore invalid");
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

#ifdef _DEBUG

void BasicSemaphore::debug_check(const BasicSemaphore* semaphore) {
    CTH_ERR(semaphore == nullptr, "semaphore must not be nullptr") throw details->exception();
    CTH_ERR(semaphore->_handle != nullptr, "semaphore must be a valid handle") throw details->exception();
}
void BasicSemaphore::debug_check_leak(const BasicSemaphore* semaphore) {
    CTH_WARN(semaphore->_handle != nullptr, "semaphore handle replaced (potential memory leak)");
}

#endif


} //namespace cth


namespace cth {
Semaphore::Semaphore(const BasicCore* core, DeletionQueue* deletion_queue) : BasicSemaphore(core), _deletionQueue(deletion_queue) {}
Semaphore::~Semaphore() { if(get() != VK_NULL_HANDLE) BasicSemaphore::destroy(_deletionQueue); }

void Semaphore::create() {
    if(get() != VK_NULL_HANDLE) BasicSemaphore::destroy(_deletionQueue);

    BasicSemaphore::create();
}
void Semaphore::destroy(DeletionQueue* deletion_queue) {
    if(deletion_queue) BasicSemaphore::destroy(deletion_queue);
    else BasicSemaphore::destroy(_deletionQueue);
}
}


