#pragma once
#include <memory>
#include <vulkan/vulkan.h>

#include <vector>

//TODO add support for more types of pipelines

namespace cth {
using namespace std;

class Device;
class PipelineLayout;

struct ShaderSpecialization;
class Shader;


class Pipeline {
public:
    struct GraphicsConfig;

    /**
    *\throws from private void create(...)
    */
    Pipeline(Device* device, const PipelineLayout* pipeline_layout, const GraphicsConfig& config_info);
    /**
    *\throws from private void create(...)
    */
    Pipeline(Device* device, const Pipeline* parent, const GraphicsConfig& config_info);

    ~Pipeline();

    void bind(VkCommandBuffer command_buffer) const;

private:
    /**
     *\throws cth::except::default_exception reason: missing render pass
     *\throws cth::except::vk_result_exception result of vkCreateGraphicsPipelines()
    */
    void create(const GraphicsConfig& config_info, const PipelineLayout* pipeline_layout = nullptr, const Pipeline* parent = nullptr);

    Device* device;
    VkPipeline vkGraphicsPipeline{};

public:
    Pipeline(const Pipeline& other) = delete;
    Pipeline(Pipeline&& other) = delete;
    Pipeline& operator=(const Pipeline& other) = delete;
    Pipeline& operator=(Pipeline&& other) = delete;
};

} // namespace cth


//Config

namespace cth {
struct Pipeline::GraphicsConfig {
    void addShaderStage(const Shader* shader, const ShaderSpecialization* specialization_info = nullptr,
        VkPipelineShaderStageCreateFlags flags = 0);
    void removeShaderStage(const Shader* shader);
    void removeShaderStage(VkShaderStageFlagBits stage);

    [[nodiscard]] VkGraphicsPipelineCreateInfo createInfo() const;

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

    static void setDefault(GraphicsConfig& config);
    static GraphicsConfig createDefault();

private:
    vector<VkPipelineShaderStageCreateInfo> vkShaderStages{};

public:
    [[nodiscard]] vector<VkPipelineShaderStageCreateInfo> shaderStages() const { return vkShaderStages; }

    friend Pipeline;
};
}
