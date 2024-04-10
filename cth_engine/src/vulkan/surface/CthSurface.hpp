#pragma once
#include <vector>
#include <vulkan/vulkan.h>

namespace cth {
using namespace std;

class Instance;
class Window;

class Surface {
public:
    explicit Surface(VkSurfaceKHR vk_surface, Instance* instance);
    ~Surface();

    [[nodiscard]] vector<VkPresentModeKHR> presentModes(VkPhysicalDevice physical_device) const;
    [[nodiscard]] vector<VkSurfaceFormatKHR> formats(VkPhysicalDevice physical_device) const;
    [[nodiscard]] VkSurfaceCapabilitiesKHR capabilities(VkPhysicalDevice physical_device) const;
    [[nodiscard]] VkBool32 support(VkPhysicalDevice physical_device, uint32_t family_index) const;

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
