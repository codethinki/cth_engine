#include "CthBaseBuffer.hpp"

#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/cmd/CthCmdBuffer.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/resource/memory/CthMemory.hpp"
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
using std::span;



BaseBuffer::BaseBuffer(cth::not_null<Core const*> core, size_t buffer_size, VkBufferUsageFlags usage_flags) :
    _core{core}, _size{buffer_size}, _usage{usage_flags} { DEBUG_CHECK_CORE(_core); }
BaseBuffer::BaseBuffer(cth::not_null<Core const*> core, size_t buffer_size, VkBufferUsageFlags usage_flags, State state) :
    BaseBuffer{core, buffer_size, usage_flags} { BaseBuffer::wrap(std::move(state)); }
BaseBuffer::BaseBuffer(cth::not_null<Core const*> core, size_t buffer_size, VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags vk_memory_flags) :
    BaseBuffer{core, buffer_size, usage_flags} { BaseBuffer::create(vk_memory_flags); }



void BaseBuffer::wrap(State state) {


    optDestroy();


    _handle = state.vkBuffer.get();
    if(state.memory) {
        DEBUG_CHECK_MEMORY(state.memory.get());
        _memory = std::move(state.memory);
    }

    _bound = state.bound;
    _mapped = state.mapped;
}

void BaseBuffer::create(VkMemoryPropertyFlags vk_memory_flags) {
    optDestroy();
    createBuffer();
    createMemory(vk_memory_flags);
    bind();
}


void BaseBuffer::destroy() {
    auto const lambda = [device = _core->vkDevice(), buffer = _handle.get()] { BaseBuffer::destroy(device, buffer); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();

    reset();
}



BaseBuffer::State BaseBuffer::release() {
    DEBUG_CHECK_BUFFER(this);
    State state{_handle.get(), std::move(_memory), _bound, _mapped};

    reset();

    return state;
}



std::span<char> BaseBuffer::map(size_t size, size_t offset) {
    CTH_ERR(size + offset > _size && size != constants::WHOLE_SIZE, "memory out of bounds") throw details->exception();


    if(!_mapped.empty() && _mapped.size() > offset + size) return span<char>{_mapped.data() + offset, size};

    auto mem = span<char>{_memory->map(size, offset).data(), size};

    if(offset == 0 && mem.size() > mapped().size()) _mapped = mem;

    return span<char>{mem.data(), size};
}
std::span<char> BaseBuffer::map() {
    _mapped = _memory->map(_size, 0);

    return _mapped;
}



void BaseBuffer::write(span<char const> data, size_t buffer_offset) const {
    CTH_ERR(_mapped.size() < data.size() + buffer_offset, "mapped region out of bounds") throw details->exception();
    std::memcpy(_mapped.data() + buffer_offset, data.data(), data.size());
}

void BaseBuffer::copy(CmdBuffer const& cmd_buffer, BaseBuffer const& src, size_t copy_size, size_t src_offset, size_t dst_offset) const {
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

void BaseBuffer::flush(size_t size, size_t offset) const { _memory->flush(size, offset); }

void BaseBuffer::invalidate(size_t size, size_t offset) const { _memory->invalidate(size, offset); }


void BaseBuffer::unmap() {
    _mapped = {};
    _memory->unmap();
}

void BaseBuffer::stage(CmdBuffer const& cmd_buffer, BaseBuffer const& staging_buffer, size_t dst_offset) const {
    debug_check(&staging_buffer);
    this->copy(cmd_buffer, staging_buffer, staging_buffer._size, 0, dst_offset);
}


VkDescriptorBufferInfo BaseBuffer::descriptorInfo(size_t size, size_t offset) const {
    return VkDescriptorBufferInfo{_handle.get(), offset, size == constants::WHOLE_SIZE ? _size : size};
}


void BaseBuffer::write(span<char const> data, span<char> mapped_memory) {
    CTH_ERR(mapped_memory.size() >= data.size(), "mapped region out of bounds") throw details->exception();
    std::memcpy(mapped_memory.data(), data.data(), data.size());
}

size_t BaseBuffer::calcAlignedSize(size_t actual_size) {
    constexpr size_t minAlignment = 16;
    return actual_size + (minAlignment - (actual_size % minAlignment));
}

void BaseBuffer::destroy(VkDevice vk_device, VkBuffer vk_buffer) {
    CTH_WARN(vk_buffer == VK_NULL_HANDLE, "vk_buffer invalid") {}
    DEBUG_CHECK_DEVICE_HANDLE(vk_device);

    vkDestroyBuffer(vk_device, vk_buffer, nullptr);
}



void BaseBuffer::reset() {
    _handle = VK_NULL_HANDLE;
    _memory = nullptr;
    _bound = false;
    _mapped = {};
}

void BaseBuffer::createBuffer() {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = _size;
    bufferInfo.usage = _usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer ptr = VK_NULL_HANDLE;

    VkResult const createResult = vkCreateBuffer(_core->vkDevice(), &bufferInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create buffer") {
        reset();
        throw result_exception{createResult, details->exception()};
    }

    _handle = ptr;
}

void BaseBuffer::createMemory(VkMemoryPropertyFlags vk_memory_properties) {
    CTH_ERR(_memory != nullptr, "memory must be empty") throw details->exception();

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(_core->vkDevice(), _handle.get(), &memRequirements);
    _memory = std::make_unique<Memory>(_core, vk_memory_properties, memRequirements);
}

void BaseBuffer::bind() {
    CTH_ERR(!_memory->created(), "memory must be allocated") throw details->exception();

    VkResult const bindResult = vkBindBufferMemory(_core->vkDevice(), _handle.get(), _memory->get(), 0);

    CTH_STABLE_ERR(bindResult != VK_SUCCESS, "failed to bind buffer memory") {
        destroy();
        throw cth::vk::result_exception{bindResult, details->exception()};
    }
}


//public
Memory* BaseBuffer::memory() const { return _memory.get(); }


#ifdef CONSTANT_DEBUG_MODE
void BaseBuffer::debug_check(BaseBuffer const* buffer) {
    CTH_ERR(buffer == nullptr, "buffer must not be nullptr") throw details->exception();
    CTH_ERR(buffer->_handle == VK_NULL_HANDLE, "buffer must be a valid handle") throw details->exception();
}
void BaseBuffer::debug_check_state(State const& state) {
    if(state.memory)
        DEBUG_CHECK_MEMORY(state.memory.get());
    else { CTH_ERR(!state.bound, "state must have created memory or be bound") throw details->exception(); }
}
#endif

} //namespace cth
