#pragma once

#include <volk.h>
#include <jvk/base/queue/queue.hpp>



namespace cth {
struct FrameResourcesConfig {
    std::string_view windowName;
    glm::uvec2 windowExtent;
    jvk::Queue presentQueue;
};
}