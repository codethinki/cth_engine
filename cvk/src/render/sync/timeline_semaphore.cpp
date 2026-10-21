#include "cvk/render/sync/timeline_semaphore.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <limits>
#include <utility>


namespace cvk {
TimelineSemaphore::TimelineSemaphore(Core const& core) : Semaphore{core} {}

TimelineSemaphore::TimelineSemaphore(Core const& core, State const& state) : TimelineSemaphore{core} {
    wrap(state);
}

TimelineSemaphore::TimelineSemaphore(Core const& core, create_t) : TimelineSemaphore{core} { create(); }

void TimelineSemaphore::wrap(State const& state) {
    Semaphore::wrap(Semaphore::State{state.vkSemaphore});
    _value = state.value;
}

void TimelineSemaphore::create(uint64_t initialValue) {
    VkSemaphoreTypeCreateInfo const typeInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = initialValue,
    };
    VkSemaphoreCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &typeInfo,
        .flags = 0,
    };
    Semaphore::create(info);

    _value = initialValue;
}

TimelineSemaphore::State TimelineSemaphore::release() {
    auto const [vkSemaphore] = Semaphore::release();
    return State{vkSemaphore, std::exchange(_value, 0)};
}

uint64_t TimelineSemaphore::signal() {
    debug_check(*this);

    auto const table = core().deviceTable();
    VkSemaphoreSignalInfo const info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .pNext = nullptr,
        .semaphore = get(),
        .value = next(),
    };
    auto const result = table->vkSignalSemaphore(table.device(), &info);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to signal semaphore") {}

    return info.value;
}

VkResult TimelineSemaphore::wait(uint64_t value, uint64_t timeout) const {
    debug_check(*this);

    auto const table = core().deviceTable();
    auto const vkSemaphore = get();

    VkSemaphoreWaitInfo const info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = nullptr,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &vkSemaphore,
        .pValues = &value,
    };
    auto const result = table->vkWaitSemaphores(table.device(), &info, timeout);

    CVK_RESULT_STABLE_THROW(
        result != VK_SUCCESS && result != VK_TIMEOUT,
        result,
        "failed to wait for semaphore"
    ) {}

    return result;
}

void TimelineSemaphore::wait(uint64_t value) const {
    [[maybe_unused]] auto const result = wait(value, std::numeric_limits<uint64_t>::max());
}

uint64_t TimelineSemaphore::gpuValue() const {
    debug_check(*this);

    auto const table = core().deviceTable();
    uint64_t value = 0;
    auto const result = table->vkGetSemaphoreCounterValue(table.device(), get(), &value);

    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to get semaphore counter value") {}

    return value;
}
}
