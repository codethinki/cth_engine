#pragma once
#include <vector>
#include "HlcDevice.hpp"
namespace cth {
	using namespace std;
	struct PipelineConfigInfo {
		PipelineConfigInfo() = default;
		PipelineConfigInfo(const PipelineConfigInfo&) = delete;
		PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

		VkPipelineViewportStateCreateInfo viewportInfo{};
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
		VkPipelineMultisampleStateCreateInfo multisampleInfo{};
		VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};

		vector<VkDynamicState> dynamicStates;
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};

		VkPipelineLayout pipelineLayout = nullptr;
		VkRenderPass renderPass = nullptr;
		uint32_t subpassCount = 0;
	};
	class HlcPipeline {
	public:
		Device& hlcDevice;
		VkPipeline vkGraphicsPipeline{};

		void createGraphicsPipeline(const PipelineConfigInfo& config_info);

		HlcPipeline(Device& device, const PipelineConfigInfo& config_info);
		~HlcPipeline();

		HlcPipeline(const HlcPipeline&) = delete;
		HlcPipeline& operator=(const HlcPipeline&) = delete;

		void bind(VkCommandBuffer command_buffer);

		static void defaultPipelineConfigInfo(PipelineConfigInfo& config_info);

		void createShaderModule(const vector<char>& code, VkShaderModule* shader_module) const;
	};

}