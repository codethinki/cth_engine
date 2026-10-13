#pragma once
#include "render_pass_begin_config.hpp"

namespace jvk {
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