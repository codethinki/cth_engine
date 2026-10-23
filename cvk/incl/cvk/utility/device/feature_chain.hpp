#pragma once
#include "device_features.hpp"

#include "cvk/utility/types.hpp"

#include <volk.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>


namespace cvk::dev {
/**
 * @brief the Vulkan feature structs of @ref DeviceFeatures, in the order of its fields
 * @details a new struct: an entry here, a @ref feature_struct specialization, its fields in @ref DeviceFeatures
 */
using feature_structs_t = std::tuple<
    VkPhysicalDeviceFeatures2,
    VkPhysicalDeviceVulkan11Features,
    VkPhysicalDeviceVulkan12Features,
    VkPhysicalDeviceVulkan13Features,
    VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR,
    VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR
>;

inline constexpr size_t FEATURE_STRUCTS = std::tuple_size_v<feature_structs_t>;

/**
 * @brief sType, VkBool32 flags and extension of a feature struct
 * @details OFFSET: byte offset of the first flag, COUNT: flags, EXTENSION: empty -> core struct
 */
template<class T>
struct feature_struct;

template<>
struct feature_struct<VkPhysicalDeviceFeatures2> {
    static constexpr auto TYPE = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceFeatures2, features);
    static constexpr size_t COUNT = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
    static constexpr std::string_view EXTENSION{};
};

template<>
struct feature_struct<VkPhysicalDeviceVulkan11Features> {
    static constexpr auto TYPE = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess);
    static constexpr size_t COUNT =
        (offsetof(VkPhysicalDeviceVulkan11Features, shaderDrawParameters) - OFFSET) / sizeof(VkBool32) + 1;
    static constexpr std::string_view EXTENSION{};
};

template<>
struct feature_struct<VkPhysicalDeviceVulkan12Features> {
    static constexpr auto TYPE = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge);
    static constexpr size_t COUNT =
        (offsetof(VkPhysicalDeviceVulkan12Features, subgroupBroadcastDynamicId) - OFFSET) / sizeof(VkBool32) + 1;
    static constexpr std::string_view EXTENSION{};
};

template<>
struct feature_struct<VkPhysicalDeviceVulkan13Features> {
    static constexpr auto TYPE = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess);
    static constexpr size_t COUNT =
        (offsetof(VkPhysicalDeviceVulkan13Features, maintenance4) - OFFSET) / sizeof(VkBool32) + 1;
    static constexpr std::string_view EXTENSION{};
};

template<>
struct feature_struct<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR> {
    static constexpr auto TYPE = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR, swapchainMaintenance1);
    static constexpr size_t COUNT = 1;
    static constexpr std::string_view EXTENSION = VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
};

template<>
struct feature_struct<VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR> {
    static constexpr auto TYPE = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR;
    static constexpr size_t OFFSET =
        offsetof(VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR, dynamicRenderingLocalRead);
    static constexpr size_t COUNT = 1;
    static constexpr std::string_view EXTENSION = VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME;
};

static_assert(feature_struct<VkPhysicalDeviceFeatures2>::COUNT == 55);
static_assert(feature_struct<VkPhysicalDeviceVulkan11Features>::COUNT == 12);
static_assert(feature_struct<VkPhysicalDeviceVulkan12Features>::COUNT == 47);
static_assert(feature_struct<VkPhysicalDeviceVulkan13Features>::COUNT == 15);

/**
 * @brief index of the first field of T in @ref DeviceFeatures
 */
template<class T>
inline constexpr size_t feature_offset = []<class... Ts>(std::type_identity<std::tuple<Ts...>>) {
    size_t offset = 0;
    bool found = false;
    ((found = found || std::same_as<T, Ts>, offset += found ? 0 : feature_struct<Ts>::COUNT), ...);
    return offset;
}(std::type_identity<feature_structs_t>{});

static_assert(
    []<class... Ts>(std::type_identity<std::tuple<Ts...>>) {
        return (feature_struct<Ts>::COUNT + ...);
    }(std::type_identity<feature_structs_t>{}) == FEATURE_COUNT,
    "DeviceFeatures must have one field per flag of the feature structs"
);
static_assert(alignof(DeviceFeatures) == 1, "DeviceFeatures must have bool fields only");

/**
 * @return the extensions of the structs with a true bit in the features
 */
[[nodiscard]] constexpr std::vector<std::string_view> extensions_of(DeviceFeatures const& features) {
    auto const bits = to_bits(features);
    std::vector<std::string_view> extensions{};

    [&]<class... Ts>(std::type_identity<std::tuple<Ts...>>) {
        ([&] {
            if constexpr(!feature_struct<Ts>::EXTENSION.empty()) {
                auto const flags = std::span{bits}.subspan(feature_offset<Ts>, feature_struct<Ts>::COUNT);
                if(std::ranges::contains(flags, true)) extensions.push_back(feature_struct<Ts>::EXTENSION);
            }
        }(), ...);
    }(std::type_identity<feature_structs_t>{});

    return extensions;
}

/**
 * @brief the feature structs as one pNext chain for Vulkan, not copyable, not movable: the chain points into the object
 * @details an extension struct is in the chain only with its extension
 */
class FeatureChain {
public:
    /**
     * @brief every feature false
     * @param extensions of the device, the chain includes their structs
     */
    explicit FeatureChain(std::span<std::string const> extensions);

    /**
     * @brief the bits of the features
     * @param extensions of the device, the chain includes their structs
     * @pre the extensions contain @ref extensions_of() the features
     */
    FeatureChain(DeviceFeatures const&, std::span<std::string const> extensions);

    ~FeatureChain() = default;

    /**
     * @return the bits of the structs, a struct out of the chain -> false
     */
    [[nodiscard]] DeviceFeatures features() const;

    /**
     * @return the struct is in the chain
     * @tparam T one of @ref feature_structs_t
     */
    template<class T>
    [[nodiscard]] constexpr bool linked() const;

    /**
     * @brief start of the pNext chain
     */
    [[nodiscard]] constexpr auto* get(this auto& self) { return &std::get<VkPhysicalDeviceFeatures2>(self._structs); }

private:
    /**
     * @brief links the pNext members of the structs in @ref _linked
     */
    void link();

    feature_structs_t _structs;
    std::array<bool, FEATURE_STRUCTS> _linked{};

public:
    FeatureChain(FeatureChain const& other) = delete;
    FeatureChain(FeatureChain&& other) = delete;
    FeatureChain& operator=(FeatureChain const& other) = delete;
    FeatureChain& operator=(FeatureChain&& other) = delete;
};

/**
 * @brief queries the features of the device
 * @param extensions of the device, the query includes their structs
 */
[[nodiscard]] DeviceFeatures query_features(vk_not_null<VkPhysicalDevice>, std::span<std::string const> extensions);
}

namespace cvk::dev {
/**
 * @brief position of T in @ref feature_structs_t
 */
template<class T>
inline constexpr size_t feature_index = []<class... Ts>(std::type_identity<std::tuple<Ts...>>) {
    constexpr std::array same{std::same_as<T, Ts>...};
    return static_cast<size_t>(std::ranges::find(same, true) - same.begin());
}(std::type_identity<feature_structs_t>{});

template<class T>
constexpr bool FeatureChain::linked() const {
    static_assert(feature_index<T> < FEATURE_STRUCTS, "T must be a feature struct");
    return _linked[feature_index<T>];
}
}
