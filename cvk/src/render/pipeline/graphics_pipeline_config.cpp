#include "cvk/render/pipeline/graphics_pipeline_config.hpp"

#include "cvk/render/pipeline/shader/shader.hpp"

#include <algorithm>


namespace cvk {
GraphicsPipelineConfig& GraphicsPipelineConfig::addStage(Shader const& shader, VkSpecializationInfo const* specialization) {
    CTH_CRITICAL(
        std::ranges::contains(stages, shader.stage(), &VkPipelineShaderStageCreateInfo::stage),
        "stage already added"
    ) {}

    stages.push_back(shader.stageInfo(specialization));
    return *this;
}
}
