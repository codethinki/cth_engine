#include "CthTimelineSemaphore.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
TimelineSemaphore::TimelineSemaphore(cth::not_null<Core const*> core) : Semaphore{core} {}
TimelineSemaphore::TimelineSemaphore(cth::not_null<Core const*> core, State const& state) : TimelineSemaphore{core} { wrap(state); }
TimelineSemaphore::TimelineSemaphore(cth::not_null<Core const*> core, bool create) : TimelineSemaphore{core} {
    if(create) Semaphore::createHandle(TimelineSemaphore::createInfo());
}

void TimelineSemaphore::wrap(State const& state) {
    Semaphore::wrap({state.vkSemaphore});
    _value = state.value;
}
TimelineSemaphore::State TimelineSemaphore::release() {
    auto const value = _value;
    auto const [vkSemaphore] = Semaphore::release();

    return State{vkSemaphore, value};
}



size_t TimelineSemaphore::gpuValue() const {
    size_t value = 0;
    auto const result = vkGetSemaphoreCounterValue(_core->vkDevice(), get(), &value);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to get semaphore counter value")
        throw vk::result_exception{result, details->exception()};

    return value;
}
void TimelineSemaphore::signal() {
    auto const info = signalInfo(++_value);

    auto const result = vkSignalSemaphore(_core->vkDevice(), &info);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to signal semaphore")
        throw vk::result_exception{result, details->exception()};
}
VkResult TimelineSemaphore::wait(uint64_t nanoseconds) const {
    DEBUG_CHECK_SEMAPHORE(this);
    auto const handle = get();

    auto const info = waitInfo(_value, handle);

    auto const result = vkWaitSemaphores(_core->vkDevice(), &info, nanoseconds);

    return result;
}
void TimelineSemaphore::reset() {
    Semaphore::reset();
    _value = 0;
}



VkSemaphoreSignalInfo TimelineSemaphore::signalInfo(size_t const& value) const {
    VkSemaphoreSignalInfo const signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .pNext = nullptr,
        .semaphore = get(),
        .value = value,
    };
    return signalInfo;
}


VkSemaphoreWaitInfo TimelineSemaphore::waitInfo(std::span<size_t const> wait_values, std::span<VkSemaphore const> wait_semaphores) {
    CTH_ERR(wait_values.size() != wait_semaphores.size(), "wait_values size ({0}) must equal wait_semaphores size ({1}) required", wait_values.size(),
        wait_semaphores.size())
        throw details->exception();


    VkSemaphoreWaitInfo const waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = nullptr,
        .flags = 0,
        .semaphoreCount = static_cast<uint32_t>(wait_semaphores.size()),
        .pSemaphores = wait_semaphores.data(),
        .pValues = wait_values.data(),
    };
    return waitInfo;

}

VkTimelineSemaphoreSubmitInfo TimelineSemaphore::submitInfo(size_t const& wait_value, size_t const& signal_value) {
    VkTimelineSemaphoreSubmitInfo const timelineInfo{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreValueCount = 1,
        .pWaitSemaphoreValues = &wait_value,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &signal_value,
    };
    return timelineInfo;
}
VkSemaphoreWaitInfo TimelineSemaphore::waitInfo(size_t const& value, VkSemaphore const& p_semaphore) {
    VkSemaphoreWaitInfo const waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .pNext = nullptr,
        .flags = 0,
        .semaphoreCount = 1,
        .pSemaphores = &p_semaphore,
        .pValues = &value,
    };
    return waitInfo;
}


VkSemaphoreCreateInfo TimelineSemaphore::createInfo() {
    static constexpr VkSemaphoreTypeCreateInfo TIMELINE_SEMAPHORE_CREATE_INFO{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0
    };

    static constexpr VkSemaphoreCreateInfo INFO{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &TIMELINE_SEMAPHORE_CREATE_INFO,
        .flags = 0,
    };

    return INFO;
}

}
