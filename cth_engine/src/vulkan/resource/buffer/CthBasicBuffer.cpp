#include "CthBasicBuffer.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/resource/memory/CthBasicMemory.hpp"
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {
using std::span;



BasicBuffer::BasicBuffer(BasicCore const* core, size_t const buffer_size, VkBufferUsageFlags const usage_flags) :
    _core(core), _size(buffer_size), _usage(usage_flags) { init(); }
BasicBuffer::BasicBuffer(BasicCore const* core, size_t const buffer_size, VkBufferUsageFlags const usage_flags, VkBuffer vk_buffer,
    State state) : _core(core), _size(buffer_size), _usage(usage_flags), _state(std::move(state)), _handle(vk_buffer) { init(); }

void BasicBuffer::wrap(VkBuffer vk_buffer, State const& state) {
    DEBUG_CHECK_BUFFER_LEAK(this);
    DEBUG_CHECK_MEMORY_LEAK(_state.memory.get());

    _handle = vk_buffer;
    _state = state;
}

void BasicBuffer::create() {
    DEBUG_CHECK_BUFFER_LEAK(this);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = _size;
    bufferInfo.usage = _usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer ptr = VK_NULL_HANDLE;

    VkResult const createResult = vkCreateBuffer(_core->vkDevice(), &bufferInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create buffer")
        throw cth::except::vk_result_exception{createResult, details->exception()};

    _handle = ptr;
}

void BasicBuffer::alloc(BasicMemory* new_memory) {
    setMemory(new_memory);
    alloc();
}
void BasicBuffer::alloc() const {
    DEBUG_CHECK_BUFFER(this);
    DEBUG_CHECK_BUFFER_NOT_BOUND(this);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_core->vkDevice(), _handle.get(), &memRequirements);

    _state.memory->alloc(memRequirements);
}

void BasicBuffer::bind(BasicMemory* new_memory) {
    setMemory(new_memory);
    bind();
}
void BasicBuffer::bind() const {
    DEBUG_CHECK_BUFFER(this);
    DEBUG_CHECK_BUFFER_NOT_BOUND(this);

    VkResult const bindResult = vkBindBufferMemory(_core->vkDevice(), _handle.get(), _state.memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind buffer memory")
        throw cth::except::vk_result_exception{bindResult, details->exception()};
}



void BasicBuffer::destroy(DestructionQueue* destruction_queue) {
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue);

    if(destruction_queue) destruction_queue->push(_handle.get());
    else destroy(_core->vkDevice(), _handle.get());

    _handle = VK_NULL_HANDLE;

    BasicBuffer::reset();
}



std::span<char> BasicBuffer::map(size_t const size, size_t const offset) {
    CTH_ERR(size + offset > _size && size != constants::WHOLE_SIZE, "memory out of bounds") throw details->exception();


    if(!_state.mapped.empty() && _state.mapped.size() > offset + size) return span<char>{_state.mapped.data() + offset, size};

    auto mem = span<char>{_state.memory->map(size, offset).data(), size};

    if(offset == 0 && mem.size() > mapped().size()) _state.mapped = mem;

    return span<char>{mem.data(), size};
}
std::span<char> BasicBuffer::map() {
    _state.mapped = _state.memory->map(_size, 0);

    return _state.mapped;
}

void BasicBuffer::unmap() {
    _state.mapped = {};
    _state.memory->unmap();
}


void BasicBuffer::stage(CmdBuffer const& cmd_buffer, BasicBuffer const& staging_buffer, size_t const dst_offset) const {
    debug_check(&staging_buffer);
    this->copy(cmd_buffer, staging_buffer, staging_buffer._size, 0, dst_offset);
}

void BasicBuffer::write(span<char const> const data, size_t const buffer_offset) const {
    CTH_ERR(_state.mapped.size() < data.size() + buffer_offset, "mapped region out of bounds") throw details->exception();
    memcpy(_state.mapped.data() + buffer_offset, data.data(), data.size());
}

void BasicBuffer::copy(CmdBuffer const& cmd_buffer, BasicBuffer const& src, size_t const copy_size, size_t src_offset, size_t dst_offset) const {
    CTH_ERR(!(src._usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT), "src buffer usage must be marked as transfer source") throw details->exception();
    CTH_ERR(!(_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT), "dst buffer usage must be marked as transfer destination") throw details->exception();


    size_t const copySize = (copy_size == constants::WHOLE_SIZE ? std::min(src._size - src_offset, _size - dst_offset) : copy_size);

    CTH_ERR(src_offset + copySize > src._size || dst_offset + copySize > _size, "copy region out of bounds") {
        if(src_offset + copySize > src._size) {
            details->add("src buffer out of bounds");
            details->add("{0} + {1} > {2} (off + copy_size > src.size)", src_offset, copySize, src._size);
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
    vkCmdCopyBuffer(cmd_buffer.get(), src.get(), _handle.get(), 1, &copyRegion);
}

VkResult BasicBuffer::flush(size_t const size, size_t const offset) const { return _state.memory->flush(size, offset); }

VkResult BasicBuffer::invalidate(size_t const size, size_t const offset) const { return _state.memory->invalidate(size, offset); }


VkDescriptorBufferInfo BasicBuffer::descriptorInfo(size_t const size, size_t const offset) const {
    return VkDescriptorBufferInfo{_handle.get(), offset, size == constants::WHOLE_SIZE ? _size : size};
}


void BasicBuffer::write(span<char const> const data, span<char> const mapped_memory) {
    CTH_ERR(mapped_memory.size() >= data.size(), "mapped region out of bounds") throw details->exception();
    memcpy(mapped_memory.data(), data.data(), data.size());
}

void BasicBuffer::destroy(VkDevice vk_device, VkBuffer vk_buffer) {
    CTH_WARN(vk_buffer == VK_NULL_HANDLE, "vk_buffer invalid") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroyBuffer(vk_device, vk_buffer, nullptr);
}

size_t BasicBuffer::calcAlignedSize(size_t const actual_size) {
    constexpr size_t minAlignment = 16;
    return actual_size + (minAlignment - (actual_size % minAlignment));
}

void BasicBuffer::reset() {
    DEBUG_CHECK_BUFFER_LEAK(this);

    _handle = VK_NULL_HANDLE;
    _state.reset();
}
BasicMemory* BasicBuffer::releaseMemory() {
    BasicMemory* mem = _state.memory.get();
    _state.memory = nullptr;
    return mem;
}

//protected

void BasicBuffer::destroyMemory(DestructionQueue* destruction_queue) {
    CTH_ERR(_state.memory == nullptr, "memory invalid") throw details->exception();

    if(_state.memory->allocated()) _state.memory->free(destruction_queue);

    delete _state.memory.get();
    _state.memory = nullptr;
}

void BasicBuffer::setMemory(BasicMemory* new_memory) {
    CTH_ERR(new_memory != nullptr && new_memory == _state.memory.get(), "new_memory must not be current memory") throw details->exception();

    DEBUG_CHECK_BUFFER_NOT_BOUND(this);
    DEBUG_CHECK_BUFFER_MEMORY_LEAK(this);

    _state.memory = new_memory;
}


void BasicBuffer::init() const { DEBUG_CHECK_CORE(_core); }

//public
BasicMemory* BasicBuffer::memory() const { return _state.memory.get(); }
bool BasicBuffer::allocated() const { return _state.memory->allocated(); }


#ifdef CONSTANT_DEBUG_MODE
void BasicBuffer::debug_check(BasicBuffer const* buffer) {
    CTH_ERR(buffer == nullptr, "buffer must not be nullptr") throw details->exception();
    CTH_ERR(buffer->_handle == VK_NULL_HANDLE, "buffer must be a valid handle") throw details->exception();
}
void BasicBuffer::debug_check_leak(BasicBuffer const* buffer) {
    CTH_WARN(buffer->_handle != VK_NULL_HANDLE, "buffer handle replaced (potential memory leak)") {}
}
void BasicBuffer::debug_check_memory_leak(BasicBuffer const* buffer) {
    CTH_WARN(buffer->_state.memory != nullptr, "memory ptr replaced (potential memory leak)") {}
}

void BasicBuffer::debug_check_not_bound(BasicBuffer const* buffer) {
    CTH_ERR(buffer->_state.bound, "buffer must not be bound") throw details->exception();
}
#endif

} //namespace cth

//State

namespace cth::vk {
void BasicBuffer::State::reset() {
    bound = false;
    mapped = {};
}
}
