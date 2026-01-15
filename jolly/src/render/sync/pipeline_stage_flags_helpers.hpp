#pragma once

#include "jolly/render/sync/pipeline_stage_flags.hpp"

#include <volk.h>

namespace jly {
constexpr PipelineStageFlags from_vk(VkPipelineStageFlags flags) {
    return static_cast<PipelineStageFlags>(flags);
}

constexpr VkPipelineStageFlags to_vk(PipelineStageFlags flags) {
    return static_cast<VkPipelineStageFlags>(flags);
}
}
