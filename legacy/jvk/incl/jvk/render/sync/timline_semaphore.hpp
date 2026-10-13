#pragma once
#include "jvk/render/sync/semaphore.hpp"

#include <span>

namespace jvk {
class TimelineSemaphore : public Semaphore {
public:
    struct State;

    /**
     * @brief base constructor
     * @note calls @ref Semaphore::Semaphore(Core const&)
     */
    explicit TimelineSemaphore(Core const& core);

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     */
    TimelineSemaphore(Core const& core, State const& state);

    /**
     * @brief constructs and may create
     * @details calls @ref create()
     */
    explicit TimelineSemaphore(Core const& core, create_t);


    /**
     * @note calls @ref Semaphore::~Semaphore()
     */
    ~TimelineSemaphore() override = default;

    /**
     * wraps the state
     */
    void wrap(State const&);

    // ReSharper disable once CppHidingFunction
    /**
     * releases state and resets
     * @return state of object
     */
    State release();


    void signal();

    [[nodiscard]] size_t value() const { return _value; }

    /**
     * advances the cpu counter 
     * @details thread safe
     */
    [[nodiscard]] size_t next() const { return ++_value; }

    [[nodiscard]] size_t gpuValue() const;
    [[nodiscard]] VkResult wait(uint64_t nanoseconds = UINT64_MAX) const;

protected:
    void reset() override;
    VkSemaphoreCreateInfo createInfo() override;

private:
    [[nodiscard]] VkSemaphoreSignalInfo signalInfo(size_t const& value) const;

    [[nodiscard]] static VkTimelineSemaphoreSubmitInfo submitInfo(
        size_t const& wait_value,
        size_t const& signal_value
    );

    [[nodiscard]] static VkSemaphoreWaitInfo waitInfo(size_t const& value, VkSemaphore const& p_semaphore);

    [[nodiscard]] static VkSemaphoreWaitInfo waitInfo(
        std::span<size_t const> wait_values,
        std::span<VkSemaphore const> wait_semaphores
    );

    mutable std::atomic<size_t> _value = 0;

public:
    TimelineSemaphore(TimelineSemaphore const& other) = delete;
    TimelineSemaphore& operator=(TimelineSemaphore const& other) = delete;
    TimelineSemaphore(TimelineSemaphore&& other) noexcept : Semaphore{std::move(other)} {
        _value.store(other._value.load(std::memory_order_relaxed));
    }
    TimelineSemaphore& operator=(TimelineSemaphore&& other) noexcept {
        if(this == &other)
            return *this;

        Semaphore::operator=(std::move(other));
        _value.store(other._value.load(std::memory_order_relaxed));
        return *this;
    };
};
} //namespace cth


namespace jvk {
struct TimelineSemaphore::State {
    jvk::vk_not_null<VkSemaphore> vkSemaphore;
    size_t value;
};

}
