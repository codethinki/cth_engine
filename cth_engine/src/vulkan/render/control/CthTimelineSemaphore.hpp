#pragma once
#include "CthSemaphore.hpp"

#include<span>

namespace cth::vk {
class TimelineSemaphore : public Semaphore {
public:
    explicit TimelineSemaphore(BasicCore const* core, bool create = true);
    ~TimelineSemaphore() override = default;


    [[nodiscard]] size_t value() const { return _value; }
    [[nodiscard]] size_t next() { return ++_value; }

    [[nodiscard]] size_t gpuValue() const;
    void signal();
    [[nodiscard]] VkResult wait(uint64_t nanoseconds = UINT64_MAX) const;

protected:
    [[nodiscard]] VkSemaphoreSignalInfo signalInfo(size_t const& value) const;

    [[nodiscard]] static VkTimelineSemaphoreSubmitInfo submitInfo(size_t const& wait_value, size_t const& signal_value);

    [[nodiscard]] static VkSemaphoreWaitInfo waitInfo(size_t const& value, VkSemaphore const& p_semaphore);

    [[nodiscard]] static VkSemaphoreWaitInfo waitInfo(std::span<size_t const> wait_values, std::span<VkSemaphore const> wait_semaphores);



    VkSemaphoreCreateInfo createInfo() override;

private:
    size_t _value = 0;

public:
    TimelineSemaphore(TimelineSemaphore const& other) = delete;
    TimelineSemaphore& operator=(TimelineSemaphore const& other) = delete;
    TimelineSemaphore(TimelineSemaphore&& other) noexcept = default;
    TimelineSemaphore& operator=(TimelineSemaphore&& other) noexcept = default;
};
} //namespace cth
