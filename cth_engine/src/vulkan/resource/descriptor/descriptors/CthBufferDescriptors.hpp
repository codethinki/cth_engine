#pragma once
#include "../CthDescriptor.hpp"

#include "vulkan/memory/buffer/CthDefaultBuffer.hpp"

namespace cth::vk {
class BufferDescriptor : public Descriptor {
public:
    explicit BufferDescriptor(const VkDescriptorType type, const DefaultBuffer* buffer, const size_t descriptor_size = Constants::WHOLE_SIZE,
        const size_t buffer_offset = 0) : Descriptor(type), vkDescriptorInfo(buffer->descriptorInfo(descriptor_size, buffer_offset)) {}
    ~BufferDescriptor() override = 0;

private:
    VkDescriptorBufferInfo vkDescriptorInfo{};

public:
    [[nodiscard]] VkDescriptorBufferInfo bufferInfo() const override { return vkDescriptorInfo; }

    BufferDescriptor(const BufferDescriptor& other) = default;
    BufferDescriptor(BufferDescriptor&& other) = delete;
    BufferDescriptor& operator=(const BufferDescriptor& other) = default;
    BufferDescriptor& operator=(BufferDescriptor&& other) = delete;
};
inline BufferDescriptor::~BufferDescriptor() = default;


class UniformBufferDescriptor : public BufferDescriptor {
public:
    inline static constexpr VkDescriptorType TYPE = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    explicit UniformBufferDescriptor(const DefaultBuffer* buffer, const size_t descriptor_size = Constants::WHOLE_SIZE,
        const size_t descriptor_offset = 0) : BufferDescriptor(TYPE, buffer, descriptor_size, descriptor_offset) {}
};

} // namespace cth
