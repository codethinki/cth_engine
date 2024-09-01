#pragma once
#include "../CthDescriptor.hpp"

#include "vulkan/memory/buffer/CthDefaultBuffer.hpp"

namespace cth::vk {
class BufferDescriptor : public Descriptor {
public:
    explicit BufferDescriptor(VkDescriptorType  type, DefaultBuffer const* buffer, size_t const descriptor_size = Constants::WHOLE_SIZE,
        size_t const buffer_offset = 0) : Descriptor(type), vkDescriptorInfo(buffer->descriptorInfo(descriptor_size, buffer_offset)) {}
    ~BufferDescriptor() override = 0;

private:
    VkDescriptorBufferInfo vkDescriptorInfo{};

public:
    [[nodiscard]] VkDescriptorBufferInfo bufferInfo() const override { return vkDescriptorInfo; }

    BufferDescriptor(BufferDescriptor const& other) = default;
    BufferDescriptor(BufferDescriptor&& other) = delete;
    BufferDescriptor& operator=(BufferDescriptor const& other) = default;
    BufferDescriptor& operator=(BufferDescriptor&& other) = delete;
};
inline BufferDescriptor::~BufferDescriptor() = default;


class UniformBufferDescriptor : public BufferDescriptor {
public:
    inline static constexpr VkDescriptorType TYPE = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    explicit UniformBufferDescriptor(DefaultBuffer const* buffer, size_t const descriptor_size = Constants::WHOLE_SIZE,
        size_t const descriptor_offset = 0) : BufferDescriptor(TYPE, buffer, descriptor_size, descriptor_offset) {}
};

} // namespace cth
