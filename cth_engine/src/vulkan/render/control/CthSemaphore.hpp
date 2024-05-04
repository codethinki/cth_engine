#pragma once
#include <vulkan/vulkan.h>

#include <cth/cth_memory.hpp>

namespace cth {
class Device;
class DeletionQueue;

class BasicSemaphore {
public:
    explicit BasicSemaphore(Device* device);
    virtual ~BasicSemaphore() = default;

    virtual void create();
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);


    static void destroy(const Device* device, VkSemaphore vk_semaphore);

protected:
    Device* _device;

private:
    mem::basic_ptr<VkSemaphore_T> _handle{};

public:
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }

    BasicSemaphore(const BasicSemaphore& other) = default;
    BasicSemaphore(BasicSemaphore&& other) = default;
    BasicSemaphore& operator=(const BasicSemaphore& other) = default;
    BasicSemaphore& operator=(BasicSemaphore&& other) = default;

#ifdef _DEBUG
    static void debug_check(const BasicSemaphore* semaphore);
    static void debug_check_replace(const BasicSemaphore* semaphore);

#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) BasicSemaphore::debug_check(semaphore_ptr)
#define DEBUG_CHECK_SEMAPHORE_REPLACE(semaphore_ptr) BasicSemaphore::debug_check_replace(semaphore_ptr)
#else
#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) ((void)0)
#define DEBUG_CHECK_SEMAPHORE_REPLACE(semaphore_ptr)  ((void)0)
#endif

};



} //namespace cth

namespace cth {

class Semaphore : public BasicSemaphore {
public:
    explicit Semaphore(Device* device, DeletionQueue* deletion_queue);
    ~Semaphore() override;

    void create() override;
    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue;
public:
    Semaphore(const Semaphore& other) = delete;
    Semaphore(Semaphore&& other) = default;
    Semaphore& operator=(const Semaphore& other) = delete;
    Semaphore& operator=(Semaphore&& other) = default;

};

}
