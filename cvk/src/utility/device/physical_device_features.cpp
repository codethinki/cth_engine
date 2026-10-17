#include "cvk/utility/device/physical_device_features.hpp"

#include <algorithm>
#include <ranges>
#include <utility>


namespace cvk::utils {
PhysicalDeviceFeatures::PhysicalDeviceFeatures() : _chain{
    VkPhysicalDeviceFeatures2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = nullptr, .features{}},
    VkPhysicalDeviceVulkan11Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = nullptr},
    VkPhysicalDeviceVulkan12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = nullptr},
    VkPhysicalDeviceVulkan13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = nullptr},
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
        .pNext = nullptr,
        .swapchainMaintenance1 = VK_FALSE,
    },
    VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR,
        .pNext = nullptr,
        .dynamicRenderingLocalRead = VK_FALSE,
    },
} { link({}); }

PhysicalDeviceFeatures::PhysicalDeviceFeatures(
    vk_not_null<VkPhysicalDevice> vkDevice,
    std::span<std::string const> extensions
) : PhysicalDeviceFeatures{} {
    link(extensions);
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

void PhysicalDeviceFeatures::link(std::span<std::string const> extensions) {
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        ((_linked[Is] = [&] {
            constexpr auto name = dev::feature_extension<std::tuple_element_t<Is, chain_t>>::NAME;
            return name.empty() || std::ranges::contains(extensions, name);
        }()), ...);
    }(std::make_index_sequence<STRUCTS>{});

    relink();
}

void PhysicalDeviceFeatures::relink() {
    void* next = nullptr;

    // back to front: each linked struct points to the next linked one
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr size_t index = STRUCTS - 1 - Is;
            auto& features = std::get<index>(_chain);

            features.pNext = _linked[index] ? next : nullptr;
            if(_linked[index]) next = &features;
        }(), ...);
    }(std::make_index_sequence<STRUCTS>{});
}
}
