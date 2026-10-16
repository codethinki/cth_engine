#include "cvk/render/pipeline/pipeline_config.hpp"

#include "cvk/render/pipeline/shader/shader.hpp"

#include <algorithm>


namespace cvk {
PipelineConfig PipelineConfig::Default(std::span<VkFormat const> colorFormats, VkFormat depthFormat) {
    PipelineConfig config{};
    config.colorFormats.assign_range(colorFormats);
    config.colorBlendAttachments.assign(colorFormats.size(), ALPHA_BLENDING);
    config.depthFormat = depthFormat;
    return config;
}

PipelineConfig& PipelineConfig::addStage(Shader const& shader, VkSpecializationInfo const* specialization) {
    CTH_CRITICAL(
        std::ranges::contains(stages, shader.stage(), &VkPipelineShaderStageCreateInfo::stage),
        "stage already added"
    ) {}

    stages.push_back(shader.stageInfo(specialization));
    return *this;
}

PipelineConfig& PipelineConfig::removeStage(VkShaderStageFlagBits stage) {
    auto const removed = std::erase_if(
        stages,
        [stage](VkPipelineShaderStageCreateInfo const& info) { return info.stage == stage; }
    );
    CTH_CRITICAL(removed == 0, "stage not added") {}

    return *this;
}
}
