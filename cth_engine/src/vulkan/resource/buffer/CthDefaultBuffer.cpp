#include "CthDefaultBuffer.hpp"

#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


namespace cth {

DefaultBuffer::DefaultBuffer(Device* device, const size_t buffer_size, const VkBufferUsageFlags usage_flags,
    const VkMemoryPropertyFlags memory_property_flags) : device(device), _size(calcAlignedSize(buffer_size)), padding(buffer_size - _size),
    _usage(usage_flags), _memoryProperties(memory_property_flags) {
    create();
    alloc();
}

DefaultBuffer::~DefaultBuffer() {
    unmap();
    vkDestroyBuffer(device->get(), vkBuffer, nullptr);
    vkFreeMemory(device->get(), vkMemory, nullptr);
}


size_t DefaultBuffer::calcAlignedSize(const size_t actual_size) {
    constexpr size_t minAlignment = 16;
    return actual_size + (minAlignment - (actual_size % minAlignment));
}
span<char> DefaultBuffer::default_map(const size_t size, const size_t offset) {
    CTH_ERR(!vkBuffer || !vkMemory, "buffer not created yet") throw details->exception();
    CTH_ERR(size + offset > _size - padding, "memory out of bounds")
        throw details->exception();

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(device->get(), vkMemory, offset, size, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult != VK_SUCCESS, "Vk: memory mapping failed")
        throw except::vk_result_exception{mapResult, details->exception()};

    return span<char>{static_cast<char*>(mappedPtr), size};
}
span<char> DefaultBuffer::default_map() {
    CTH_ERR(mapped.data(), "buffer already mapped") throw details->exception();
    CTH_ERR(!vkBuffer || !vkMemory, "buffer not created yet") throw details->exception();

    void* mappedPtr = nullptr;
    const VkResult mapResult = vkMapMemory(device->get(), vkMemory, 0, VK_WHOLE_SIZE, 0, &mappedPtr);
    CTH_STABLE_ERR(mapResult != VK_SUCCESS, "Vk: memory mapping failed")
        throw except::vk_result_exception{mapResult, details->exception()};

    mapped = span<char>{static_cast<char*>(mappedPtr), _size - padding};
    return mapped;
}

void DefaultBuffer::unmap() {
    if(!mapped.data()) return;

    vkUnmapMemory(device->get(), vkMemory);
    mapped = span<char>{};
}

void DefaultBuffer::default_stage(const span<const char> data, const size_t buffer_offset) const {
    const unique_ptr<DefaultBuffer> stagingBuffer = make_unique<DefaultBuffer>(device, data.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer->default_map();
    stagingBuffer->default_write(data);

    this->copy(stagingBuffer.get(), stagingBuffer->_size, 0, buffer_offset);
}

void DefaultBuffer::default_write(const span<const char> data, const span<char> mapped_memory) {
    memcpy(mapped_memory.data(), data.data(), data.size());
}
void DefaultBuffer::default_write(const span<const char> data, size_t buffer_offset) const {
    CTH_ERR(!mapped.data(), "mapped_memory invalid or buffer was not mapped entirely")
        throw details->exception();

    memcpy(mapped.data() + buffer_offset, data.data(), data.size());
}
void DefaultBuffer::copy(DefaultBuffer* src, const size_t copy_size, size_t src_offset, size_t dst_offset) const {
    CTH_ERR(!(src->_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT), "src buffer usage must be marked as transfer source")
        throw details->exception();
    CTH_ERR(!(_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT), "dst buffer usage must be marked as transfer destination")
        throw details->exception();


    const size_t copySize = copy_size == VK_WHOLE_SIZE ? min(src->_size - src_offset, _size - dst_offset) : copy_size;

    CTH_ERR(src_offset + copySize > src->_size || dst_offset + copySize > _size, "copy operation out of bounds") {
        if(src_offset + copySize > src->_size) {
            details->add("src buffer out of bounds");
            details->add("{0} + {1} > {2} (off + copy_size > src.size)", src_offset, copySize, src->_size);
        }
        if(dst_offset + copySize > _size) {
            details->add("dst buffer out of bounds");
            details->add("{0} + {1} > {2} (off + copy_size > dst.size)", dst_offset, copySize, _size);
        }
        throw details->exception();
    }

    VkCommandBuffer commandBuffer = src->device->beginSingleTimeCommands();

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = src_offset;
    copyRegion.dstOffset = dst_offset;
    copyRegion.size = _size;
    vkCmdCopyBuffer(commandBuffer, src->get(), vkBuffer, 1, &copyRegion);

    src->device->endSingleTimeCommands(commandBuffer);
}

VkResult DefaultBuffer::flush(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkFlushMappedMemoryRanges(device->get(), 1, &mappedRange);
}

VkDescriptorBufferInfo DefaultBuffer::descriptorInfo(const size_t size, const size_t offset) const {
    return VkDescriptorBufferInfo{vkBuffer, offset, size == VK_WHOLE_SIZE ? _size : size};
}

VkResult DefaultBuffer::invalidate(const size_t size, const size_t offset) const {
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = vkMemory;
    mappedRange.offset = offset;
    mappedRange.size = size;
    return vkInvalidateMappedMemoryRanges(device->get(), 1, &mappedRange);
}

void DefaultBuffer::create() {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = _size;
    bufferInfo.usage = _usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createResult = vkCreateBuffer(device->get(), &bufferInfo, nullptr, &vkBuffer);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create buffer")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}
void DefaultBuffer::alloc() {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device->get(), vkBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = device->findMemoryType(memRequirements.memoryTypeBits, _memoryProperties);

    const VkResult allocResult = vkAllocateMemory(device->get(), &allocInfo, nullptr, &vkMemory);

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate buffer memory")
        throw cth::except::vk_result_exception{allocResult, details->exception()};

    vkBindBufferMemory(device->get(), vkBuffer, vkMemory, 0);
}



} //namespace cth
