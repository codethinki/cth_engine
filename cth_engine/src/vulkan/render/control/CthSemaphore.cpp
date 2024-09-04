#include "CthSemaphore.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {


Semaphore::Semaphore(cth::not_null<BasicCore const*> core) : _core(core) { DEBUG_CHECK_CORE(core); }
Semaphore::Semaphore(cth::not_null<BasicCore const*> core, State const& state) : Semaphore{core} { wrap(state); }
Semaphore::Semaphore(cth::not_null<BasicCore const*> core, bool create) : Semaphore{core} { if(create) this->create(); }

void Semaphore::wrap(State const& state) {
    optDestroy();
    _handle = state.vkSemaphore.get();
}
void Semaphore::create() {
    optDestroy();
    createHandle(createInfo());
}

void Semaphore::destroy() {
    DEBUG_CHECK_SEMAPHORE(this);

    auto const queue = _core->destructionQueue();

    if(queue) queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    reset();
}


Semaphore::State Semaphore::release() {
    State const state{_handle.get()};
    reset();
    return state;
}
void Semaphore::destroy(VkDevice vk_device, VkSemaphore vk_semaphore) {
    CTH_WARN(vk_semaphore == VK_NULL_HANDLE, "vk_semaphore invalid") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroySemaphore(vk_device, vk_semaphore, nullptr);
}

VkSemaphoreCreateInfo Semaphore::createInfo() {
    constexpr VkSemaphoreCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    return info;
}
void Semaphore::createHandle(VkSemaphoreCreateInfo const& info) {
    VkSemaphore ptr = VK_NULL_HANDLE;
    auto const createResult = vkCreateSemaphore(_core->vkDevice(), &info, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create semaphore") {
        reset();
        throw cth::vk::result_exception{createResult, details->exception()};
    }
    _handle = ptr;
}
void Semaphore::reset() { _handle = nullptr; }



#ifdef CONSTANT_DEBUG_MODE
void Semaphore::debug_check(cth::not_null<Semaphore const*> semaphore) {
    CTH_ERR(!semaphore->created(), "semaphore must be created") throw details->exception();
}

#endif

} //namespace cth
