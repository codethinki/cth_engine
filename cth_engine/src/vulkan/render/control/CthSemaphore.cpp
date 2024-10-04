#include "CthSemaphore.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {


Semaphore::Semaphore(cth::not_null<Core const*> core) : _core(core) { Core::debug_check(core); }
Semaphore::Semaphore(cth::not_null<Core const*> core, State const& state) : Semaphore{core} { wrap(state); }
Semaphore::Semaphore(cth::not_null<Core const*> core, bool create) : Semaphore{core} { if(create) this->create(); }

void Semaphore::wrap(State const& state) {
    optDestroy();
    _handle = state.vkSemaphore.get();
}
void Semaphore::create() {
    optDestroy();
    createHandle(createInfo());
}

void Semaphore::destroy() {
    debug_check(this);

    auto const lambda = [vk_device = _core->vkDevice(), vk_semaphore = _handle.get()]() { destroy(vk_device, vk_semaphore); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}


Semaphore::State Semaphore::release() {
    debug_check(this);

    State const state{_handle.get()};
    reset();
    return state;
}
void Semaphore::destroy(vk::not_null<VkDevice> vk_device, VkSemaphore vk_semaphore) {
    CTH_WARN(vk_semaphore == VK_NULL_HANDLE, "vk_semaphore invalid") {}
    Device::debug_check_handle(vk_device);

    vkDestroySemaphore(vk_device.get(), vk_semaphore, nullptr);
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

}
