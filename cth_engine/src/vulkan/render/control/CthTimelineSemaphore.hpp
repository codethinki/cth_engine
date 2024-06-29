#pragma once
#include "CthSemaphore.hpp"

#include<span>

namespace cth {
class TimelineSemaphore : public Semaphore {
public:
    explicit TimelineSemaphore(const BasicCore* core, DeletionQueue* deletion_queue) : Semaphore(core, deletion_queue) {}
    ~TimelineSemaphore() override = default;


    [[nodiscard]] size_t value() const { return _value; }
    [[nodiscard]] size_t next() { return ++_value; }

    [[nodiscard]] size_t gpuValue() const;
    void signal();
    [[nodiscard]] VkResult wait(uint64_t nanoseconds = UINT64_MAX) const;

protected:
    [[nodiscard]] VkSemaphoreSignalInfo signalInfo(const size_t& value) const;

    [[nodiscard]] static VkTimelineSemaphoreSubmitInfo submitInfo(const size_t& wait_value, const size_t& signal_value);

    [[nodiscard]] static VkSemaphoreWaitInfo waitInfo(const size_t& value, const VkSemaphore& p_semaphore);

    [[nodiscard]] static VkSemaphoreWaitInfo waitInfo(std::span<const size_t> wait_values, std::span<const VkSemaphore> wait_semaphores);

    VkSemaphoreCreateInfo createInfo() override;

private:
    size_t _value = 0;

public:
    TimelineSemaphore(const TimelineSemaphore& other) = delete;
    TimelineSemaphore& operator=(const TimelineSemaphore& other) = delete;
    TimelineSemaphore(TimelineSemaphore&& other) noexcept = default;
    TimelineSemaphore& operator=(TimelineSemaphore&& other) noexcept = default;
};
} //namespace cth
