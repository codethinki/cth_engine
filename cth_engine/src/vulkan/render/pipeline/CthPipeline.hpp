#pragma once
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

//TODO add support for more types of pipelines

namespace cth::vk {
class BasicCore;

class PipelineLayout;

struct ShaderSpecialization;
class Shader;
class CmdBuffer;


class Pipeline {
public:
    struct GraphicsConfig;

    /**
    *@throws from private void create()
    */
    Pipeline(const BasicCore* core, const PipelineLayout* pipeline_layout, const GraphicsConfig& config_info);
    /**
    *@throws from private void create()
    */
    Pipeline(const BasicCore* core, const Pipeline* parent, const GraphicsConfig& config_info);

    ~Pipeline();

    void bind(const CmdBuffer* cmd_buffer) const;

private:
    /**
     *@throws cth::except::default_exception reason: missing render pass
     *@throws cth::except::vk_result_exception result of vkCreateGraphicsPipelines()
    */
    void create(const GraphicsConfig& config_info, const PipelineLayout* pipeline_layout = nullptr, const Pipeline* parent = nullptr);

    const BasicCore* _device;
    VkPipeline _vkGraphicsPipeline{};

public:
    Pipeline(const Pipeline& other) = delete;
    Pipeline(Pipeline&& other) = delete;
    Pipeline& operator=(const Pipeline& other) = delete;
    Pipeline& operator=(Pipeline&& other) = delete;
};

} // namespace cth


//Config

namespace cth::vk {
struct Pipeline::GraphicsConfig {
    void addShaderStage(const Shader* shader, const ShaderSpecialization* specialization_info = nullptr,
        VkPipelineShaderStageCreateFlags flags = 0);
    void removeShaderStage(const Shader* shader);
    void removeShaderStage(VkShaderStageFlagBits shader_stage);

    [[nodiscard]] VkGraphicsPipelineCreateInfo createInfo() const;

    std::unique_ptr<VkPipelineVertexInputStateCreateInfo> vertexInputInfo{};
    std::unique_ptr<VkPipelineInputAssemblyStateCreateInfo> inputAssemblyInfo{};
    std::unique_ptr<VkPipelineRasterizationStateCreateInfo> rasterizationInfo{};
    std::unique_ptr<VkPipelineMultisampleStateCreateInfo> multisampleInfo{};
    std::unique_ptr<VkPipelineColorBlendAttachmentState> colorBlendAttachment{};
    std::unique_ptr<VkPipelineColorBlendStateCreateInfo> colorBlendInfo{};
    std::unique_ptr<VkPipelineDepthStencilStateCreateInfo> depthStencilInfo{};
    std::unique_ptr<VkPipelineViewportStateCreateInfo> viewportInfo{};

    std::vector<VkDynamicState> dynamicStates{};
    std::unique_ptr<VkPipelineDynamicStateCreateInfo> dynamicStateInfo{};

    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t subpassCount = 0;

    static void setDefault(GraphicsConfig& config);
    static GraphicsConfig createDefault();

private:
    std::vector<VkPipelineShaderStageCreateInfo> _vkShaderStages{};

public:
    [[nodiscard]] std::vector<VkPipelineShaderStageCreateInfo> shaderStages() const { return _vkShaderStages; }

    friend Pipeline;
};
}
