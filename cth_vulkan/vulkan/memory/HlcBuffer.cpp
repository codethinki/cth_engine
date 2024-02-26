#include "HlcBuffer.hpp"

#include <cassert>
#include <cstring>


namespace cth {
/**
 * Returns the minimum instance size required to be compatible with devices minOffsetAlignment
 *
 * @param instance_size The size of an instance
 * @param min_offset_alignment The minimum required alignment, in bytes, for the offset member (eg
 * minUniformBufferOffsetAlignment)
 *
 * @return VkResult of the buffer mapping call
 */
VkDeviceSize Buffer::getAlignment(const VkDeviceSize instance_size, const VkDeviceSize min_offset_alignment) {
	if(min_offset_alignment > 0) { return (instance_size + min_offset_alignment - 1) & ~(min_offset_alignment - 1); }
	return instance_size;
}

Buffer::Buffer(Device& device, const VkDeviceSize instance_size, const uint32_t instance_count, const VkBufferUsageFlags usage_flags,
	const VkMemoryPropertyFlags memory_property_flags, const VkDeviceSize min_offset_alignment) : hlcDevice{device},
	instanceSize{instance_size},
	instanceCount{instance_count},
	usageFlags{usage_flags},
	memoryPropertyFlags{memory_property_flags} {
	alignmentSize = getAlignment(instance_size, min_offset_alignment);
	bufferSize = alignmentSize * instance_count;
	device.createBuffer(bufferSize, usage_flags, memory_property_flags, buffer, memory);
}

Buffer::~Buffer() {
	unmap();
	vkDestroyBuffer(hlcDevice.device(), buffer, nullptr);
	vkFreeMemory(hlcDevice.device(), memory, nullptr);
}

/**
 * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
 *
 * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
 * buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the buffer mapping call
 */
VkResult Buffer::map(const VkDeviceSize size, const VkDeviceSize offset) {
	assert(buffer && memory && "Called map on buffer before create");
	return vkMapMemory(hlcDevice.device(), memory, offset, size, 0, &mapped);
}

/**
 * Unmap a mapped memory range
 *
 * @note Does not return a result as vkUnmapMemory can't fail
 */
void Buffer::unmap() {
	if(mapped) {
		vkUnmapMemory(hlcDevice.device(), memory);
		mapped = nullptr;
	}
}

/**
 * @note Allocates memory with staging Buffer
 * @param data Pointer to memory to create the staging Buffer from
 */
void Buffer::stage(const void* data) const {
	Buffer stagingBuffer = Buffer(hlcDevice,
		instanceSize,
		instanceCount,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingBuffer.map();
	stagingBuffer.writeToBuffer(data);
	hlcDevice.copyBuffer(stagingBuffer.getBuffer(), this->getBuffer(), bufferSize);
}


/**
 * Copies the specified data to the mapped buffer. Default value writes whole buffer range
 *
 * @param data Pointer to the data to copy
 * @param size byte size of new data
 * @param data_offset byte offset of data ptr
 * @param mapped_offset byte offset of mapped ptr
 *
 */
void Buffer::writeToBuffer(const void* data, const VkDeviceSize size, const VkDeviceSize data_offset, const VkDeviceSize mapped_offset) const {
	assert(mapped && "writeToBuffer: Cannot copy to unmapped buffer");

	if(size == VK_WHOLE_SIZE) memcpy(mapped, data, bufferSize);
	else memcpy(static_cast<char*>(mapped) + mapped_offset, static_cast<const char*>(data) + data_offset, size);

}

/**
 * Flush a memory range of the buffer to make it visible to the device
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the
 * complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the flush call
 */
VkResult Buffer::flush(const VkDeviceSize size, const VkDeviceSize offset) const {
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	return vkFlushMappedMemoryRanges(hlcDevice.device(), 1, &mappedRange);
}

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
 * the complete buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::invalidate(const VkDeviceSize size, const VkDeviceSize offset) const {
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	return vkInvalidateMappedMemoryRanges(hlcDevice.device(), 1, &mappedRange);
}

/**
 * Create a buffer info descriptor
 *
 * @param size (Optional) Size of the memory range of the descriptor
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkDescriptorBufferInfo of specified offset and range
 */
VkDescriptorBufferInfo Buffer::descriptorInfo(const VkDeviceSize size, const VkDeviceSize offset) const {
	return VkDescriptorBufferInfo{
		buffer,
		offset,
		size,
	};
}

/**
 * Copies "instanceSize" bytes of data to the mapped buffer at an offset of index * alignmentSize
 *
 * @param data Pointer to the data to copy
 * @param index Used in offset calculation
 *
 */
void Buffer::writeToIndex(const void* data, const int index) const { writeToBuffer(data, instanceSize, index * alignmentSize); }

/**
 *  Flush the memory range at index * alignmentSize of the buffer to make it visible to the device
 *
 * @param index Used in offset calculation
 *
 */
VkResult Buffer::flushIndex(const int index) const { return flush(alignmentSize, index * alignmentSize); }

/**
 * Create a buffer info descriptor
 *
 * @param index Specifies the region given by index * alignmentSize
 *
 * @return VkDescriptorBufferInfo for instance at index
 */
VkDescriptorBufferInfo Buffer::descriptorInfoForIndex(const int index) const { return descriptorInfo(alignmentSize, index * alignmentSize); }

/**
 * Invalidate a memory range of the buffer to make it visible to the host
 *
 * @note Only required for non-coherent memory
 *
 * @param index Specifies the region to invalidate: index * alignmentSize
 *
 * @return VkResult of the invalidate call
 */
VkResult Buffer::invalidateIndex(const int index) const { return invalidate(alignmentSize, index * alignmentSize); }
}