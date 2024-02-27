#pragma once
#include "..\core\CthDevice.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace cth {

	class HlcDescriptorSetLayout {
	public:
		class Builder {
		public:
			Builder(Device& hlc_device) : hlcDevice{ hlc_device } {}
			Builder& addBinding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count = 1);
			[[nodiscard]] std::unique_ptr<HlcDescriptorSetLayout> build() const;

		private:
			Device& hlcDevice;
			std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
		};

		HlcDescriptorSetLayout(
			Device& hlc_device, const std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings);
		~HlcDescriptorSetLayout();
		HlcDescriptorSetLayout(const HlcDescriptorSetLayout&) = delete;
		HlcDescriptorSetLayout& operator=(const HlcDescriptorSetLayout&) = delete;

        [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

	private:
		Device& hlcDevice;
		VkDescriptorSetLayout descriptorSetLayout;
		std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

		friend class HlcDescriptorWriter;
	};

	class HlcDescriptorPool {
	public:
		class Builder {
		public:
			Builder(Device& hlc_device) : hlcDevice{ hlc_device } {}

			Builder& addPoolSize(VkDescriptorType descriptor_type, uint32_t count);
			Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);
			Builder& setMaxSets(uint32_t count);
			[[nodiscard]]std::unique_ptr<HlcDescriptorPool> build(VkDescriptorType descriptor_type, uint32_t max_sets, uint32_t pool_size);
			[[nodiscard]] std::unique_ptr<HlcDescriptorPool> build() const;

		private:
			Device& hlcDevice;
			std::vector<VkDescriptorPoolSize> poolSizes{};
			uint32_t maxSets = 1000;
			VkDescriptorPoolCreateFlags poolFlags = 0;
		};

		HlcDescriptorPool(Device& hlc_device, uint32_t max_sets, VkDescriptorPoolCreateFlags pool_flags, const std::vector<VkDescriptorPoolSize>& pool_sizes);
		~HlcDescriptorPool();
		HlcDescriptorPool(const HlcDescriptorPool&) = delete;
		HlcDescriptorPool& operator=(const HlcDescriptorPool&) = delete;

		bool allocateDescriptorSet(
			VkDescriptorSetLayout descriptor_set_layout, VkDescriptorSet& descriptor) const;

		void freeDescriptors(const std::vector<VkDescriptorSet>& descriptors) const;

		void resetPool() const;
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	private:
		Device& hlcDevice;
		VkDescriptorPool descriptorPool;
		friend class HlcDescriptorWriter;
	};

	class HlcDescriptorWriter {
	public:
		HlcDescriptorWriter(HlcDescriptorSetLayout& set_layout, HlcDescriptorPool& pool);

		HlcDescriptorWriter& writeBuffer(uint32_t binding, const VkDescriptorBufferInfo* buffer_info);
		HlcDescriptorWriter& writeImage(uint32_t binding, const VkDescriptorImageInfo* image_info);

		bool build(VkDescriptorSet& set);
		void overwrite(const VkDescriptorSet& set);

	private:
		HlcDescriptorSetLayout& setLayout;
		HlcDescriptorPool& pool;
		std::vector<VkWriteDescriptorSet> writes;
	};

}