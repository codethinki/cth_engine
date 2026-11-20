#include "cvk/data/descriptor/heap_buffer.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk::dev {
void create_heap_buffer(BaseBuffer& buffer, size_t size, size_t alignment, size_t maxSize, std::string_view heap) {
    Core::debug_check(buffer.core());
    CTH_CRITICAL(size == 0, "{} heap size must not be 0", heap) {}

    buffer.optDestroy();

    CVK_STABLE_THROW(size > maxSize, "{} heap size exceeds the device limit, got {} of {}", heap, size, maxSize) {}

    auto config = HEAP_BUFFER_CONFIG;
    config.memory.minAlignment = alignment;
    buffer = BaseBuffer{buffer.core(), config, size};

    CTH_WARN(
        (buffer.memory()->properties() & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0,
        "{} heap memory is not device local, the gpu fetches the descriptors from host memory",
        heap
    ) {}
}

VkBindHeapInfoEXT heap_bind_info(BaseBuffer const& buffer, size_t reservedOffset, size_t reservedSize) {
    BaseBuffer::debug_check(buffer);

    return VkBindHeapInfoEXT{
        .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
        .pNext = nullptr,
        .heapRange = {.address = buffer.address().get(), .size = buffer.size()},
        .reservedRangeOffset = reservedOffset,
        .reservedRangeSize = reservedSize,
    };
}
}
