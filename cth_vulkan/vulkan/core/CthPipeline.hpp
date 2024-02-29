#pragma once
#include <vulkan/vulkan.h>

#include <vector>

namespace cth {
using namespace std;

class Device;
struct PipelineConfigInfo {
    PipelineConfigInfo() = default;

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
class Pipeline {
public:
    /**
     *\throws cth::except::data_exception data: config_info
     *\throws cth::except::vk_result_exception result of vkCreateGraphicsPipelines()
     */
    void createGraphicsPipeline(const PipelineConfigInfo& config_info);

    /**
     *\throws cth::except::vk_result_exception
     */
    Pipeline(Device* device, const PipelineConfigInfo& config_info);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void bind(VkCommandBuffer command_buffer) const;

    static void defaultPipelineConfigInfo(PipelineConfigInfo& config_info);

    /**
     *\throws cth::except::vk_result_exception result of vkCreateShaderModule()
     */
    void createShaderModule(const vector<char>& code, VkShaderModule* shader_module) const;

private:
    Device* device;
    VkPipeline vkGraphicsPipeline{};
};

}
