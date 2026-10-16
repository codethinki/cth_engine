#include "cvk/render/sync/semaphore.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
Semaphore::Semaphore(Core const& core) : _core{&core} { Core::debug_check(core); }

Semaphore::Semaphore(Core const& core, State const& state) : Semaphore{core} { wrap(state); }

Semaphore::Semaphore(Core const& core, create_t) : Semaphore{core} { create(); }

void Semaphore::wrap(State const& state) {
    optDestroy();

    _handle = state.vkSemaphore.get();
}

void Semaphore::create() {
    constexpr VkSemaphoreCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    create(info);
}

void Semaphore::destroy() {
    debug_check(*this);

    destroy(_core->deviceTable(), _handle.get());

    reset();
}

Semaphore::State Semaphore::release() {
    debug_check(*this);

    State const state{_handle.get()};
    reset();
    return state;
}

void Semaphore::destroy(DeviceTable table, VkSemaphore vkSemaphore) {
    CTH_WARN(vkSemaphore == VK_NULL_HANDLE, "vkSemaphore invalid (VK_NULL_HANDLE)") {}

    table->vkDestroySemaphore(table.device(), vkSemaphore, nullptr);
}

void Semaphore::create(VkSemaphoreCreateInfo const& info) {
    optDestroy();

    VkSemaphore ptr = VK_NULL_HANDLE;
    auto const result = _core->functions()->vkCreateSemaphore(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create semaphore") {
        reset();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle = ptr;
}

void Semaphore::reset() { _handle = VK_NULL_HANDLE; }
}
