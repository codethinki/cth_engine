#pragma once
#include <vulkan/vulkan.h>

#include<cth/cth_pointer.hpp>

namespace cth {
class BasicCore;
class Device;
class DeletionQueue;

class BasicSemaphore {
public:
    explicit BasicSemaphore(const BasicCore* core);
    virtual ~BasicSemaphore() = default;

    virtual void create();
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);


    static void destroy(VkDevice vk_device, VkSemaphore vk_semaphore);

protected:
    virtual VkSemaphoreCreateInfo createInfo();
    virtual void createHandle(const VkSemaphoreCreateInfo& info);

    const BasicCore* _core;

private:
    move_ptr<VkSemaphore_T> _handle{};

public:
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }

    BasicSemaphore(const BasicSemaphore& other) = default;
    BasicSemaphore(BasicSemaphore&& other) = default;
    BasicSemaphore& operator=(const BasicSemaphore& other) = default;
    BasicSemaphore& operator=(BasicSemaphore&& other) = default;

#ifdef _DEBUG
    static void debug_check(const BasicSemaphore* semaphore);
    static void debug_check_leak(const BasicSemaphore* semaphore);

#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) BasicSemaphore::debug_check(semaphore_ptr)
#define DEBUG_CHECK_SEMAPHORE_LEAK(semaphore_ptr) BasicSemaphore::debug_check_leak(semaphore_ptr)
#else
#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) ((void)0)
#define DEBUG_CHECK_SEMAPHORE_LEAK(semaphore_ptr)  ((void)0)
#endif

};



} //namespace cth

namespace cth {

class Semaphore : public BasicSemaphore {
public:
    explicit Semaphore(const BasicCore* core, DeletionQueue* deletion_queue, bool create = true);
    ~Semaphore() override;

    void create() override;
    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue;

public:
    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;
    Semaphore(Semaphore&& other) noexcept = default;
    Semaphore& operator=(Semaphore&& other) noexcept = default;

};

}



