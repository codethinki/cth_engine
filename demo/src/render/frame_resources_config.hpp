#pragma once

#include <volk.h>
#include <jvk/base/queue/queue.hpp>



namespace cth {
struct FrameResourcesConfig {
    std::string_view windowName;
    VkExtent2D windowExtent;
    jvk::Queue presentQueue;
};
}