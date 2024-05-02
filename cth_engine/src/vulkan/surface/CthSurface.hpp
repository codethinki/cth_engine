#pragma once
#include "vulkan/base/CthInstance.hpp"

#include <cth/cth_log.hpp>


namespace cth {
using namespace std;

class Instance;
class Window;


class Surface {
public:
    explicit Surface(VkSurfaceKHR vk_surface, Instance* instance) : _vkSurface(vk_surface), _instance(instance) {}
    ~Surface() {
        vkDestroySurfaceKHR(_instance->get(), _vkSurface, nullptr);
        log::msg("destroyed surface");
    }

private:
    VkSurfaceKHR _vkSurface = VK_NULL_HANDLE;

    Instance* _instance;

public:
    [[nodiscard]] VkSurfaceKHR get() const { return _vkSurface; }

    Surface(const Surface& other) = delete;
    Surface(Surface&& other) = delete;
    Surface& operator=(const Surface& other) = delete;
    Surface& operator=(Surface&& other) = delete;
};
}
