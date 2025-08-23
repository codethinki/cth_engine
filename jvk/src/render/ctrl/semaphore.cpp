#include "jvk/render/ctrl/semaphore.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/device.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"


namespace jvk {


Semaphore::Semaphore(Core const& core) : _core{&core} { Core::debug_check(core); }
Semaphore::Semaphore(Core const& core, State const& state) : Semaphore{core} { wrap(state); }
Semaphore::Semaphore(Core const& core, create_t) : Semaphore{core} { create(); }

void Semaphore::wrap(State const& state) {
    optDestroy();
    _handle = state.vkSemaphore.get();
}

void Semaphore::create() {
    optDestroy();
    createHandle(createInfo());
}

void Semaphore::destroy() {
    debug_check(*this);

    auto const lambda = [table = _core->deviceTable(), vk_semaphore = _handle.get()]() {
        destroy(table, vk_semaphore);
    };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}


Semaphore::State Semaphore::release() {
    debug_check(*this);

    State const state{_handle.get()};
    reset();
    return state;
}

void Semaphore::destroy(DeviceTable table, VkSemaphore vk_semaphore) {
    CTH_WARN(vk_semaphore == VK_NULL_HANDLE, "vk_semaphore should not be invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySemaphore(table.device(), vk_semaphore, nullptr);
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
    auto const createResult = _core->deviceTable()->
                                     vkCreateSemaphore(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create semaphore") {
        reset();
        throw jvk::result_exception{createResult, details->exception()};
    }
    _handle = ptr;
}

void Semaphore::reset() { _handle = nullptr; }

}