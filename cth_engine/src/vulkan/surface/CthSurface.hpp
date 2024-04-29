#pragma once
#include "vulkan/base/CthInstance.hpp"

#include <cth/cth_log.hpp>


namespace cth {
using namespace std;

class Instance;
class Window;


class Surface {
public:
    explicit Surface(VkSurfaceKHR vk_surface, Instance* instance) : vkSurface(vk_surface), instance(instance) {}
    ~Surface() {
        vkDestroySurfaceKHR(instance->get(), vkSurface, nullptr);
        log::msg("destroyed surface");
    }

private:
    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

    Instance* instance;

public:
    [[nodiscard]] VkSurfaceKHR get() const { return vkSurface; }

    Surface(const Surface& other) = delete;
    Surface(Surface&& other) = delete;
    Surface& operator=(const Surface& other) = delete;
    Surface& operator=(Surface&& other) = delete;
};
}
