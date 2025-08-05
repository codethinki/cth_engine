#pragma once
#include "RenderPassBeginConfig.hpp"

namespace cth::vk {
class Subpass;

struct RenderPassConfig {
    using BeginConfig = RenderPassBeginConfig;

    std::vector<Subpass const*> subpasses;
    std::vector<VkSubpassDependency> dependencies;
    /**
     * @brief optional, can be specified after create
     */
    BeginConfig beginConfig{};
};
}
