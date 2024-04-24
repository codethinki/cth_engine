#pragma once


#include <vulkan/vulkan_core.h>

#include <span>

namespace cth {
class DeletionQueue;
}

namespace cth {
class Device;

using std::span;

class BasicMemory {
public:
    BasicMemory(Device* device, const VkMemoryPropertyFlags properties) : device(device), vkProperties(properties) {}
    BasicMemory(Device* device, const VkMemoryPropertyFlags properties, const size_t size, VkDeviceMemory memory) : device(device),
        vkProperties(properties), _size(size), vkMemory(memory) { debug_check(this); }
    virtual ~BasicMemory() = default;

    virtual void alloc(const VkMemoryRequirements& requirements);

    [[nodiscard]] span<char> map(size_t map_size = VK_WHOLE_SIZE, size_t offset = 0) const;
    VkResult flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;

    [[nodiscard]] VkResult invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0) const;
    void unmap() const;

    virtual void free(DeletionQueue* deletion_queue = nullptr);

    static void free(const Device* device, VkDeviceMemory memory);

protected:
    void reset();

private:
    Device* device;
    VkMemoryPropertyFlags vkProperties;
    size_t _size = 0;
    VkDeviceMemory vkMemory = VK_NULL_HANDLE;

public:
    [[nodiscard]] bool allocated() const { return vkMemory != VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceMemory get() const { return vkMemory; }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] VkMemoryPropertyFlags properties() const { return vkProperties; }

    BasicMemory(const BasicMemory& other) = delete;
    BasicMemory(BasicMemory&& other) = delete;
    BasicMemory& operator=(const BasicMemory& other) = delete;
    BasicMemory& operator=(BasicMemory&& other) = delete;
#ifdef _DEBUG
    static void debug_check(const BasicMemory* memory);
#else
    void debug_check() const {};
#endif

};

} // namespace cth

namespace cth {

class Memory : public BasicMemory {
public:
    Memory(Device* device, DeletionQueue* deletion_queue, const VkMemoryPropertyFlags properties) : BasicMemory(device, properties),
        deletionQueue(deletion_queue) {}
    Memory(Device* device, DeletionQueue* deletion_queue, const VkMemoryPropertyFlags properties, const size_t size, VkDeviceMemory memory) :
        BasicMemory(device, properties, size, memory), deletionQueue(deletion_queue) {}
    ~Memory() override;

    void free(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* deletionQueue;

};
}
