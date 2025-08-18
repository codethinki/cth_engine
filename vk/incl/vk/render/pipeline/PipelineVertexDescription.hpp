#pragma once
namespace cth::vk {
    struct PipelineVertexDescription {
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;
    };
}
