#pragma once
#include "CthBasicMemory.hpp"

namespace cth::vk {
class Memory : public BasicMemory {
public:
    Memory(BasicCore const* core, DeletionQueue* deletion_queue, VkMemoryPropertyFlags properties);
    Memory(BasicCore const* core, DeletionQueue* deletion_queue, VkMemoryPropertyFlags properties, size_t size, VkDeviceMemory memory);
    ~Memory() override;

    void alloc(VkMemoryRequirements const& requirements) override;

    void free(DeletionQueue* deletion_queue = nullptr) override;

    void reset() override;

private:
    DeletionQueue* _deletionQueue;

public:
    Memory(Memory const& other) = delete;
    Memory(Memory&& other) = default;
    Memory& operator=(Memory const& other) = delete;
    Memory& operator=(Memory&& other) = default;
};
}
