#pragma once
namespace jvk {
struct PipelineVertexDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
};
}