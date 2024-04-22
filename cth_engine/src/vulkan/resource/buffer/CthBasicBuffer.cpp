#include "CthBasicBuffer.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"

#include "../CthMemory.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


namespace cth {

BasicBuffer::BasicBuffer(Device* device, const size_t buffer_size, const VkBufferUsageFlags usage_flags, Memory* memory) :
    device(device), _memory(memory), _size(buffer_size), _usage(usage_flags) {}

void BasicBuffer::create() {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = _size;
    bufferInfo.usage = _usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createResult = vkCreateBuffer(device->get(), &bufferInfo, nullptr, &vkBuffer);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create buffer")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}
void BasicBuffer::alloc() const {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device->get(), vkBuffer, &memRequirements);

    _memory->alloc(memRequirements);
}
void BasicBuffer::bind() const {
    const VkResult bindResult = vkBindBufferMemory(device->get(), vkBuffer, _memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind buffer memory")
        throw cth::except::vk_result_exception{bindResult, details->exception()};
}

size_t BasicBuffer::calcAlignedSize(const size_t actual_size) {
    constexpr size_t minAlignment = 16;
    return actual_size + (minAlignment - (actual_size % minAlignment));
}
span<char> BasicBuffer::map(const size_t size, const size_t offset) {
    CTH_ERR(size + offset > _size && size != VK_WHOLE_SIZE, "memory out of bounds") throw details->exception();


    if(!mapped.empty()) return span<char>{mapped.data() + offset, size};

    auto mem = span<char>{_memory->map(size, offset).data(), size};

    if(size == VK_WHOLE_SIZE && offset == 0) mapped = mem;

    return span<char>{mem.data(), size};
}
span<char> BasicBuffer::map() {
    if(mapped.empty()) mapped = map(VK_WHOLE_SIZE, 0);

    return mapped;
}
void BasicBuffer::unmap() {
    mapped = {};
    _memory->unmap();
}


void BasicBuffer::stage(const CmdBuffer* cmd_buffer, const BasicBuffer* staging_buffer, const size_t dst_offset) const {
    this->copy(cmd_buffer, staging_buffer, staging_buffer->_size, 0, dst_offset);
}

void BasicBuffer::write(const span<const char> data, const span<char> mapped_memory) { memcpy(mapped_memory.data(), data.data(), data.size()); }
void BasicBuffer::write(const span<const char> data, const size_t buffer_offset) const {
    CTH_ERR(!mapped.data(), "mapped_memory invalid or buffer was not mapped entirely")
        throw details->exception();

    memcpy(mapped.data() + buffer_offset, data.data(), data.size());
}
void BasicBuffer::copy(const CmdBuffer* cmd_buffer, const BasicBuffer* src, const size_t copy_size, size_t src_offset, size_t dst_offset) const {
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

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = src_offset;
    copyRegion.dstOffset = dst_offset;
    copyRegion.size = _size;
    vkCmdCopyBuffer(cmd_buffer->get(), src->get(), vkBuffer, 1, &copyRegion);
}

VkResult BasicBuffer::flush(const size_t size, const size_t offset) const {
    return _memory->flush(size, offset);
}
VkResult BasicBuffer::invalidate(const size_t size, const size_t offset) const { return _memory->invalidate(size, offset); }



VkDescriptorBufferInfo BasicBuffer::descriptorInfo(const size_t size, const size_t offset) const {
    return VkDescriptorBufferInfo{vkBuffer, offset, size == VK_WHOLE_SIZE ? _size : size};
}

void BasicBuffer::destroy() {
    BasicBuffer::destroy(device, vkBuffer);
    reset();
}
void BasicBuffer::destroy(const Device* device, VkBuffer vk_buffer) { vkDestroyBuffer(device->get(), vk_buffer, nullptr); }
void BasicBuffer::reset() {
    vkBuffer = VK_NULL_HANDLE;
    mapped = {};
}




} //namespace cth
