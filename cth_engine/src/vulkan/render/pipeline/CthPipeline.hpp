#pragma once
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

//TODO add support for more types of pipelines

//TEMP modernize this

namespace cth::vk {
class Core;

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
    Pipeline(cth::not_null<Core const*> core, PipelineLayout const* pipeline_layout, GraphicsConfig const& config_info);
    /**
    *@throws from private void create()
    */
    Pipeline(cth::not_null<Core const*> core, Pipeline const* parent, GraphicsConfig const& config_info);

    ~Pipeline();

    void bind(CmdBuffer const* cmd_buffer) const;

private:
    /**
     *@throws cth::except::default_exception reason: missing render pass
     *@throws cth::vk::result_exception result of @ref vkCreateGraphicsPipelines()
    */
    void create(GraphicsConfig const& config_info, PipelineLayout const* pipeline_layout = nullptr, Pipeline const* parent = nullptr);

    cth::not_null<Core const*> _device;
    VkPipeline _vkGraphicsPipeline{};

public:
    Pipeline(Pipeline const& other) = delete;
    Pipeline(Pipeline&& other) = delete;
    Pipeline& operator=(Pipeline const& other) = delete;
    Pipeline& operator=(Pipeline&& other) = delete;
};

} // namespace cth


//Config

namespace cth::vk {
struct Pipeline::GraphicsConfig {
    void addShaderStage(Shader const* shader, ShaderSpecialization const* specialization_info = nullptr,
        VkPipelineShaderStageCreateFlags flags = 0);
    void removeShaderStage(Shader const* shader);
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
