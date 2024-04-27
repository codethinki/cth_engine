#include "CthBasicBuffer.hpp"

#include "../CthDeletionQueue.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"

#include "../CthMemory.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/utility/CthConstants.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


namespace cth {

BasicBuffer::BasicBuffer(Device* device, const size_t buffer_size, const VkBufferUsageFlags usage_flags) :
    device(device), size_(buffer_size), usage_(usage_flags) { init(); }
BasicBuffer::BasicBuffer(Device* device, const size_t buffer_size, const VkBufferUsageFlags usage_flags, VkBuffer vk_buffer, const State& state) :
    device(device),
    size_(buffer_size), usage_(usage_flags), vkBuffer(vk_buffer), state_(state) { init(); }

void BasicBuffer::create() {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size_;
    bufferInfo.usage = usage_;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkResult createResult = vkCreateBuffer(device->get(), &bufferInfo, nullptr, &vkBuffer);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create buffer")
        throw cth::except::vk_result_exception{createResult, details->exception()};
}

void BasicBuffer::alloc(BasicMemory* new_memory) {
    setMemory(new_memory);
    alloc();
}
void BasicBuffer::alloc() const {
    debug_check(this);
    debug_check_not_bound(this);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device->get(), vkBuffer, &memRequirements);

    state_.memory->alloc(memRequirements);
}

void BasicBuffer::bind(BasicMemory* new_memory) {
    setMemory(new_memory);
    bind();
}
void BasicBuffer::bind() const {
    debug_check(this);
    debug_check_not_bound(this);

    const VkResult bindResult = vkBindBufferMemory(device->get(), vkBuffer, state_.memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind buffer memory")
        throw cth::except::vk_result_exception{bindResult, details->exception()};
}



void BasicBuffer::destroy(DeletionQueue* deletion_queue) {

    if(!deletion_queue) destroy(device, vkBuffer);
    else deletion_queue->push(vkBuffer);
    reset();
}

void BasicBuffer::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory == state_.memory, "new_memory must not be current memory") throw details->exception();
    CTH_ERR(new_memory == nullptr, "new_memory must not be nullptr") throw details->exception();
    debug_check_not_bound(this);
    state_.memory = new_memory;
}


span<char> BasicBuffer::map(const size_t size, const size_t offset) {
    CTH_ERR(size + offset > size_ && size != Constants::WHOLE_SIZE, "memory out of bounds") throw details->exception();


    if(!state_.mapped.empty() && state_.mapped.size() > offset + size) return span<char>{state_.mapped.data() + offset, size};

    auto mem = span<char>{state_.memory->map(size, offset).data(), size};

    if(offset == 0 && mem.size() > mapped().size()) state_.mapped = mem;

    return span<char>{mem.data(), size};
}
span<char> BasicBuffer::map() {
    state_.mapped = state_.memory->map(size_, 0);

    return state_.mapped;
}

void BasicBuffer::unmap() {
    state_.mapped = {};
    state_.memory->unmap();
}


void BasicBuffer::stage(const CmdBuffer& cmd_buffer, const BasicBuffer& staging_buffer, const size_t dst_offset) const {
    debug_check(&staging_buffer);
    this->copy(cmd_buffer, staging_buffer, staging_buffer.size_, 0, dst_offset);
}

void BasicBuffer::write(const span<const char> data, const size_t buffer_offset) const {
    CTH_ERR(state_.mapped.size() < data.size() + buffer_offset, "mapped region out of bounds") throw details->exception();
    memcpy(state_.mapped.data() + buffer_offset, data.data(), data.size());
}

void BasicBuffer::copy(const CmdBuffer& cmd_buffer, const BasicBuffer& src, const size_t copy_size, size_t src_offset, size_t dst_offset) const {
    CTH_ERR(!(src.usage_ & VK_BUFFER_USAGE_TRANSFER_SRC_BIT), "src buffer usage must be marked as transfer source") throw details->exception();
    CTH_ERR(!(usage_ & VK_BUFFER_USAGE_TRANSFER_DST_BIT), "dst buffer usage must be marked as transfer destination") throw details->exception();


    const size_t copySize = copy_size == Constants::WHOLE_SIZE ? min(src.size_ - src_offset, size_ - dst_offset) : copy_size;

    CTH_ERR(src_offset + copySize > src.size_ || dst_offset + copySize > size_, "copy region out of bounds") {
        if(src_offset + copySize > src.size_) {
            details->add("src buffer out of bounds");
            details->add("{0} + {1} > {2} (off + copy_size > src.size)", src_offset, copySize, src.size_);
        }
        if(dst_offset + copySize > size_) {
            details->add("dst buffer out of bounds");
            details->add("{0} + {1} > {2} (off + copy_size > dst.size)", dst_offset, copySize, size_);
        }
        throw details->exception();
    }

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = src_offset;
    copyRegion.dstOffset = dst_offset;
    copyRegion.size = size_;
    vkCmdCopyBuffer(cmd_buffer.get(), src.get(), vkBuffer, 1, &copyRegion);
}

VkResult BasicBuffer::flush(const size_t size, const size_t offset) const { return state_.memory->flush(size, offset); }

VkResult BasicBuffer::invalidate(const size_t size, const size_t offset) const { return state_.memory->invalidate(size, offset); }


VkDescriptorBufferInfo BasicBuffer::descriptorInfo(const size_t size, const size_t offset) const {
    return VkDescriptorBufferInfo{vkBuffer, offset, size == Constants::WHOLE_SIZE ? size_ : size};
}


void BasicBuffer::write(const span<const char> data, const span<char> mapped_memory) {
    CTH_ERR(mapped_memory.size() >= data.size(), "mapped region out of bounds") throw details->exception();
    memcpy(mapped_memory.data(), data.data(), data.size());
}

void BasicBuffer::destroy(const Device* device, VkBuffer vk_buffer) {
    CTH_WARN(vk_buffer == VK_NULL_HANDLE, "vk_buffer invalid");
    Device::debug_check(device);
    vkDestroyBuffer(device->get(), vk_buffer, nullptr);
}

size_t BasicBuffer::calcAlignedSize(const size_t actual_size) {
    constexpr size_t minAlignment = 16;
    return actual_size + (minAlignment - (actual_size % minAlignment));
}

void BasicBuffer::reset() {
    vkBuffer = VK_NULL_HANDLE;
    state_.reset();
}


void BasicBuffer::init() const { Device::debug_check(device); }

#ifdef _DEBUG
void BasicBuffer::debug_check(const BasicBuffer* buffer) {
    CTH_ERR(buffer == nullptr, "buffer must not be nullptr") throw details->exception();
    CTH_ERR(buffer->vkBuffer == VK_NULL_HANDLE, "buffer must be a valid handle") throw details->exception();
}
void BasicBuffer::debug_check_not_bound(const BasicBuffer* buffer) {
    CTH_ERR(buffer->state_.bound, "buffer must not be bound") throw details->exception();
}
#endif

} //namespace cth

//State

namespace cth {
void BasicBuffer::State::reset() {
    bound = false;
    mapped = {};
}
}
