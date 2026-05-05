#pragma once

#include <volk.h>
#include <jolly/render/submit/queue.hpp>


namespace cth {
struct FrameResourcesConfig {
    std::string_view windowName;
    glm::uvec2 windowExtent;
    jly::not_null<jly::Queue const*> presentQueue;
};
}