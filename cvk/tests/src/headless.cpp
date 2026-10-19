#include "headless.hpp"

#include "cvk/utility/constant/device_constants.hpp"

#include <algorithm>
#include <ranges>


namespace cvk::test {
std::vector<std::string> headless_extensions() {
    std::vector<std::string> extensions{
        std::from_range,
        constants::PRESENT_INSTANCE_EXTENSIONS | std::views::transform(
            [](std::string_view extension) { return std::string{extension}; }
        )
    };
    extensions.emplace_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
    return extensions;
}

Surface headless_surface(Instance const& instance, Surface::Config config) {
    VkHeadlessSurfaceCreateInfoEXT const createInfo{
        .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
    };

    VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
    auto const result = vkCreateHeadlessSurfaceEXT(instance.get(), &createInfo, nullptr, &vkSurface);
    CTH_STABLE_THROW(result != VK_SUCCESS, "failed to create headless surface") {}

    return Surface{instance, std::move(config), Surface::State{vkSurface}};
}

PhysicalDevicePick pick(
    Instance const& instance,
    std::span<QueueFamilyProperties const> queueProperties,
    std::span<Surface const> surfaces
) {
    auto features = constants::BASE_DEVICE_FEATURES;
    std::vector<std::string> extensions{};

    bool const present = std::ranges::any_of(
        queueProperties,
        [](QueueFamilyProperties properties) { return contains(properties, QueueFamilyProperties::PRESENT); }
    );
    if(present) {
        features.merge(constants::PRESENT_DEVICE_FEATURES);
        for(auto const extension : constants::PRESENT_DEVICE_EXTENSIONS)
            extensions.emplace_back(extension);
    }

    return pick_physical_device(instance, surfaces, queueProperties, features, extensions).value();
}
}
