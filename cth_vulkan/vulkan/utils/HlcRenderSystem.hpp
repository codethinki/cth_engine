#pragma once
#include "HlcFrameInfo.hpp"
#include  "..\core\CthDevice.hpp"
#include "..\core\CthPipeline.hpp"
#include "../memory/HlcDescriptor.hpp"
#include "..\models\HlcOldModel.hpp"

#include <memory>
#include <vector>

#include "../memory/HlcMemoryManager.hpp"


namespace cth {
class MemoryManager;
using namespace std;

class RenderObject;

class RenderSystem {
public:
	struct RenderData {
		explicit RenderData(vector<unique_ptr<RenderObject>>& objects) : objects(objects) {}

		array<size_t, 4> groupSizes{};

		vector<uint32_t> groupIndices{};
		vector<unique_ptr<RenderObject>>& objects;
	};

	RenderSystem(Device& device, MemoryManager& memory_manager, VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);
	~RenderSystem();

	RenderSystem(const RenderSystem&) = delete;
	RenderSystem& operator=(const RenderSystem&) = delete;

	double detail = 75;
	VkExtent2D windowSize = {1000, 1000};

	void render(const RenderData& render_data, FrameInfo& frame_info) const;
	void updateDynamicChunks() const { memoryManager.updateDynamicChunks(); }

private:
	//void initSamplers();
	void initDescriptorUtils();
	void initDescriptorSets();
	void initDescriptedBuffers(vector<VkDescriptorBufferInfo>& buffer_infos);
	//void initDescriptedTextures(uint32_t& descriptor_set_index, vector<VkDescriptorImageInfo>& image_infos);

	void createPipelineLayout();
	void createPipeline(VkRenderPass render_pass, VkSampleCountFlagBits msaa_samples);

	Device& hlcDevice;
	MemoryManager& memoryManager;

	unique_ptr<CthDescriptorPool> descriptorPool{};
	unique_ptr<HlcDescriptorSetLayout> descriptorSetLayout{};
	//unique_ptr<HlcTextureSampler> defaultTextureSampler;
	vector<VkDescriptorSet> descriptorSets;
	vector<unique_ptr<Buffer>> descriptedBuffers;

	//vector<unique_ptr<Image>> descriptedImages;
	unique_ptr<Pipeline> hlcPipeline;
	VkPipelineLayout pipelineLayout{};
};
}