#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

//TODO add better encapsulation for the configs

namespace cth {
using namespace std;
class Pipeline;
class Shader;
struct ShaderSpecialization;

class GraphicsPipelineConfig {
public:
    void addShaderStage(const Shader* shader, const ShaderSpecialization* specialization_info = nullptr, VkPipelineShaderStageCreateFlags flags = 0);
    void removeShaderStage(const Shader* shader);
    void removeShaderStage(VkShaderStageFlagBits stage);

    unique_ptr<VkPipelineVertexInputStateCreateInfo> vertexInputInfo{};
    unique_ptr<VkPipelineInputAssemblyStateCreateInfo> inputAssemblyInfo{};
    unique_ptr<VkPipelineRasterizationStateCreateInfo> rasterizationInfo{};
    unique_ptr<VkPipelineMultisampleStateCreateInfo> multisampleInfo{};
    unique_ptr<VkPipelineColorBlendAttachmentState> colorBlendAttachment{};
    unique_ptr<VkPipelineColorBlendStateCreateInfo> colorBlendInfo{};
    unique_ptr<VkPipelineDepthStencilStateCreateInfo> depthStencilInfo{};
    unique_ptr<VkPipelineViewportStateCreateInfo> viewportInfo{};

    vector<VkDynamicState> dynamicStates{};
    unique_ptr<VkPipelineDynamicStateCreateInfo> dynamicStateInfo{};

    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpassCount = 0;

    static void setDefault(GraphicsPipelineConfig& config);
    static GraphicsPipelineConfig createDefault();

private:
    vector<VkPipelineShaderStageCreateInfo> vkShaderStages{};

public:
    [[nodiscard]] vector<VkPipelineShaderStageCreateInfo> shaderStages() const { return vkShaderStages; }

    friend Pipeline;
};

}
