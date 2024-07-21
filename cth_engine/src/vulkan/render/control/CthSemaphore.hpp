#pragma once
#include <vulkan/vulkan.h>

#include<cth/cth_pointer.hpp>

namespace cth::vk {
class BasicCore;
class Device;
class DeletionQueue;

class BasicSemaphore {
public:
    explicit BasicSemaphore(BasicCore const* core);
    virtual ~BasicSemaphore() = default;

    virtual void create();
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);


    static void destroy(VkDevice vk_device, VkSemaphore vk_semaphore);

protected:
    virtual VkSemaphoreCreateInfo createInfo();
    virtual void createHandle(VkSemaphoreCreateInfo const& info);

    BasicCore const* _core;

private:
    move_ptr<VkSemaphore_T> _handle{};

public:
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }

    BasicSemaphore(BasicSemaphore const& other) = default;
    BasicSemaphore(BasicSemaphore&& other) = default;
    BasicSemaphore& operator=(BasicSemaphore const& other) = default;
    BasicSemaphore& operator=(BasicSemaphore&& other) = default;

#ifdef _DEBUG
    static void debug_check(BasicSemaphore const* semaphore);
    static void debug_check_leak(BasicSemaphore const* semaphore);

#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) BasicSemaphore::debug_check(semaphore_ptr)
#define DEBUG_CHECK_SEMAPHORE_LEAK(semaphore_ptr) BasicSemaphore::debug_check_leak(semaphore_ptr)
#else
#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) ((void)0)
#define DEBUG_CHECK_SEMAPHORE_LEAK(semaphore_ptr)  ((void)0)
#endif

};



} //namespace cth

namespace cth::vk {

class Semaphore : public BasicSemaphore {
public:
    explicit Semaphore(BasicCore const* core, DeletionQueue* deletion_queue, bool create = true);
    ~Semaphore() override;

    void create() override;
    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue;

public:
    Semaphore(Semaphore const& other) = delete;
    Semaphore& operator=(Semaphore const& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

};

}



