#pragma once
#include "..\core\CthDevice.hpp"


namespace cth {
	class Buffer {
	public:
		Buffer(Device& device, VkDeviceSize instance_size, uint32_t instance_count, VkBufferUsageFlags usage_flags,
				  VkMemoryPropertyFlags memory_property_flags, VkDeviceSize min_offset_alignment = 1);
		~Buffer();

        Buffer(const Buffer& other) = delete;
        Buffer& operator=(const Buffer& other) = delete;
        Buffer(Buffer&& other) = delete;
        Buffer& operator=(Buffer&& other) = delete;

        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void unmap();
		void stage(const void* data) const;

		void writeToBuffer(const void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize data_offset = 0, VkDeviceSize mapped_offset = 0) const;

        [[nodiscard]] VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
        [[nodiscard]] VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
        [[nodiscard]] VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;

		void writeToIndex(const void* data, int index) const;
        [[nodiscard]] VkResult flushIndex(int index) const;
        [[nodiscard]] VkDescriptorBufferInfo descriptorInfoForIndex(int index) const;
        [[nodiscard]] VkResult invalidateIndex(int index) const;

		[[nodiscard]] VkBuffer getBuffer() const { return buffer; }
		[[nodiscard]] void* getMappedMemory() const { return mapped; }
		[[nodiscard]] uint32_t getInstanceCount() const { return instanceCount; }
		[[nodiscard]] VkDeviceSize getInstanceSize() const { return instanceSize; }
		[[nodiscard]] VkDeviceSize getAlignmentSize() const { return instanceSize; }
		[[nodiscard]] VkBufferUsageFlags getUsageFlags() const { return usageFlags; }
		[[nodiscard]] VkMemoryPropertyFlags getMemoryPropertyFlags() const { return memoryPropertyFlags; }
		[[nodiscard]] VkDeviceSize getBufferSize() const { return bufferSize; }

	private:
		static VkDeviceSize getAlignment(VkDeviceSize instance_size, VkDeviceSize min_offset_alignment);

		Device& hlcDevice;
		void* mapped = nullptr;
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;

		VkDeviceSize bufferSize;
		uint32_t instanceCount;
		VkDeviceSize instanceSize;
		VkDeviceSize alignmentSize;
		VkBufferUsageFlags usageFlags;
		VkMemoryPropertyFlags memoryPropertyFlags;
	};

}