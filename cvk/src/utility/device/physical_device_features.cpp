#include "cvk/utility/device/physical_device_features.hpp"

#include <ranges>


namespace cvk::utils {
PhysicalDeviceFeatures::PhysicalDeviceFeatures() : _chain{
    VkPhysicalDeviceFeatures2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = nullptr, .features{}},
    VkPhysicalDeviceVulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = nullptr},
    VkPhysicalDeviceVulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = nullptr},
    VkPhysicalDeviceVulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = nullptr},
} { link(); }

PhysicalDeviceFeatures::PhysicalDeviceFeatures(vk_not_null<VkPhysicalDevice> vkDevice) : PhysicalDeviceFeatures{} {
    vkGetPhysicalDeviceFeatures2(vkDevice.get(), &features());
}

auto PhysicalDeviceFeatures::supports(
    PhysicalDeviceFeatures const& requiredFeatures
) const -> std::vector<MissingFeature> {
    std::vector<MissingFeature> missingFeatures{};

    auto const check = [&]<class T>(T const& available, T const& required) {
        auto const flags = std::views::zip(dev::flags(available), dev::flags(required));

        for(auto const [index, flag] : std::views::enumerate(flags))
            if(std::get<1>(flag) && !std::get<0>(flag))
                missingFeatures.emplace_back(available.sType, static_cast<size_t>(index));
    };

    std::apply(
        [&]<class... Ts>(Ts const&... available) { (check(available, std::get<Ts>(requiredFeatures._chain)), ...); },
        _chain
    );

    return missingFeatures;
}

void PhysicalDeviceFeatures::merge(PhysicalDeviceFeatures const& other) {
    auto const merge = [&]<class T>(T& to, T const& from) {
        for(auto [toFlag, fromFlag] : std::views::zip(dev::flags(to), dev::flags(from)))
            toFlag |= fromFlag;
    };

    std::apply([&]<class... Ts>(Ts&... to) { (merge(to, std::get<Ts>(other._chain)), ...); }, _chain);
}

void PhysicalDeviceFeatures::link() {
    auto& [features2, features11, features12, features13] = _chain;

    features2.pNext = &features11;
    features11.pNext = &features12;
    features12.pNext = &features13;
    features13.pNext = nullptr;
}
}
