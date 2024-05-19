#pragma once
#include "CthBasicMemory.hpp"

namespace cth {
class Memory : public BasicMemory {
public:
    Memory(const BasicCore* core, DeletionQueue* deletion_queue, VkMemoryPropertyFlags properties);
    Memory(const BasicCore* core, DeletionQueue* deletion_queue, VkMemoryPropertyFlags properties, size_t size, VkDeviceMemory memory);
    ~Memory() override;

    void alloc(const VkMemoryRequirements& requirements) override;

    void free(DeletionQueue* deletion_queue = nullptr) override;

    void reset() override;

private:
    DeletionQueue* _deletionQueue;

public:
    Memory(const Memory& other) = delete;
    Memory(Memory&& other) = default;
    Memory& operator=(const Memory& other) = delete;
    Memory& operator=(Memory&& other) = default;
};
}
