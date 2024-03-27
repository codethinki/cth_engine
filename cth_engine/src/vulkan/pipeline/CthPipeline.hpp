#pragma once
#include <vulkan/vulkan.h>

#include <vector>

//TODO add support for more types of pipelines

namespace cth {
using namespace std;

class PipelineLayout;
class GraphicsPipelineConfig;
class Device;


class Pipeline {
public:
    /**
    *\throws from private void create(...)
    */
    Pipeline(Device* device, const PipelineLayout* pipeline_layout, const GraphicsPipelineConfig& config_info);
    /**
    *\throws from private void create(...)
    */
    Pipeline(Device* device, const Pipeline* parent, const GraphicsPipelineConfig& config_info);

    ~Pipeline();

    void bind(VkCommandBuffer command_buffer) const;

private:
    /**
     *\throws cth::except::default_exception reason: missing render pass
     *\throws cth::except::vk_result_exception result of vkCreateGraphicsPipelines()
    */
    void create(const GraphicsPipelineConfig& config_info, const PipelineLayout* pipeline_layout = nullptr, const Pipeline* parent = nullptr);

    Device* device;
    VkPipeline vkGraphicsPipeline{};

public:
    Pipeline(const Pipeline& other) = delete;
    Pipeline(Pipeline&& other) = delete;
    Pipeline& operator=(const Pipeline& other) = delete;
    Pipeline& operator=(Pipeline&& other) = delete;
};

}
