#pragma once
#include "cvk/utility/format.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/string/format.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>


namespace cvk::utils {
/**
 * @brief feature that a device does not have
 * @details @ref index is the position in the VkBool32 flags of the struct with @ref sType
 */
struct MissingFeature {
    VkStructureType sType;
    size_t index;
};

/**
 * @brief features of the vulkan 1.0 to 1.3 core and of extensions as one pNext chain
 * @details copyable, copy and move link the chain again,
 *  an extension struct is in the chain only after @ref link() with its extension
 */
class PhysicalDeviceFeatures {
public:
    using chain_t = std::tuple<
        VkPhysicalDeviceFeatures2,
        VkPhysicalDeviceVulkan11Features,
        VkPhysicalDeviceVulkan12Features,
        VkPhysicalDeviceVulkan13Features,
        VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR,
        VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR
    >;

    static constexpr size_t STRUCTS = std::tuple_size_v<chain_t>;

    /**
     * @brief every feature false
     * @post the core structs are in the chain
     */
    PhysicalDeviceFeatures();

    /**
     * @brief queries the features of the device
     * @param extensions of the device, the query includes their structs
     * @post the core structs and the structs of the extensions are in the chain
     */
    PhysicalDeviceFeatures(vk_not_null<VkPhysicalDevice>, std::span<std::string const> extensions);

    ~PhysicalDeviceFeatures() = default;

    /**
     * @return features that @ref requiredFeatures enables and this does not
     */
    [[nodiscard]] std::vector<MissingFeature> supports(PhysicalDeviceFeatures const& requiredFeatures) const;

    /**
     * @brief enables every feature that @ref other enables
     */
    void merge(PhysicalDeviceFeatures const& other);

    /**
     * @brief puts the core structs and the structs of the extensions in the chain
     * @details the structs of other extensions stay out, Vulkan forbids them
     */
    void link(std::span<std::string const> extensions);

    /**
     * @return the struct is in the chain
     * @tparam T one of @ref chain_t
     */
    template<class T>
    [[nodiscard]] constexpr bool linked() const;

    /**
     * @tparam T one of @ref chain_t
     */
    template<class T = VkPhysicalDeviceFeatures2>
    [[nodiscard]] constexpr auto& features(this auto& self) { return std::get<T>(self._chain); }

    /**
     * @brief start of the pNext chain, for VkDeviceCreateInfo::pNext
     */
    [[nodiscard]] constexpr VkPhysicalDeviceFeatures2 const* get() const {
        return &std::get<VkPhysicalDeviceFeatures2>(_chain);
    }

private:
    /**
     * @brief links the pNext members of the structs in @ref _linked
     */
    void relink();

    chain_t _chain;
    std::array<bool, STRUCTS> _linked{};

public:
    PhysicalDeviceFeatures(PhysicalDeviceFeatures const& other) : _chain{other._chain}, _linked{other._linked} {
        relink();
    }
    PhysicalDeviceFeatures(PhysicalDeviceFeatures&& other) noexcept : _chain{other._chain}, _linked{other._linked} {
        relink();
    }
    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures const& other) {
        _chain = other._chain;
        _linked = other._linked;
        relink();
        return *this;
    }
    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures&& other) noexcept {
        _chain = other._chain;
        _linked = other._linked;
        relink();
        return *this;
    }
};
}

namespace cvk::utils::dev {
/**
 * @brief position of the VkBool32 flags in a feature struct
 */
template<class T>
struct feature_flags;

template<>
struct feature_flags<VkPhysicalDeviceFeatures2> {
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceFeatures2, features);
    static constexpr size_t COUNT = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
};

template<>
struct feature_flags<VkPhysicalDeviceVulkan11Features> {
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess);
    static constexpr size_t COUNT =
        (offsetof(VkPhysicalDeviceVulkan11Features, shaderDrawParameters) - OFFSET) / sizeof(VkBool32) + 1;
};

template<>
struct feature_flags<VkPhysicalDeviceVulkan12Features> {
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge);
    static constexpr size_t COUNT =
        (offsetof(VkPhysicalDeviceVulkan12Features, subgroupBroadcastDynamicId) - OFFSET) / sizeof(VkBool32) + 1;
};

template<>
struct feature_flags<VkPhysicalDeviceVulkan13Features> {
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess);
    static constexpr size_t COUNT =
        (offsetof(VkPhysicalDeviceVulkan13Features, maintenance4) - OFFSET) / sizeof(VkBool32) + 1;
};

template<>
struct feature_flags<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR> {
    static constexpr size_t OFFSET = offsetof(VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR, swapchainMaintenance1);
    static constexpr size_t COUNT = 1;
};

template<>
struct feature_flags<VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR> {
    static constexpr size_t OFFSET =
        offsetof(VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR, dynamicRenderingLocalRead);
    static constexpr size_t COUNT = 1;
};

static_assert(feature_flags<VkPhysicalDeviceFeatures2>::COUNT == 55);
static_assert(feature_flags<VkPhysicalDeviceVulkan11Features>::COUNT == 12);
static_assert(feature_flags<VkPhysicalDeviceVulkan12Features>::COUNT == 47);
static_assert(feature_flags<VkPhysicalDeviceVulkan13Features>::COUNT == 15);

/**
 * @brief extension of a feature struct, core struct -> empty
 */
template<class T>
struct feature_extension {
    static constexpr std::string_view NAME{};
};

template<>
struct feature_extension<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR> {
    static constexpr std::string_view NAME = VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
};

template<>
struct feature_extension<VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR> {
    static constexpr std::string_view NAME = VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME;
};

/**
 * @brief position of T in the chain
 */
template<class T>
constexpr size_t chain_index = []<class... Ts>(std::type_identity<std::tuple<Ts...>>) {
    constexpr std::array same{std::same_as<T, Ts>...};
    return static_cast<size_t>(std::ranges::find(same, true) - same.begin());
}(std::type_identity<PhysicalDeviceFeatures::chain_t>{});

/**
 * @return the VkBool32 flags of @ref features
 */
template<class T>
[[nodiscard]] auto flags(T& features) {
    using feature_t = std::remove_const_t<T>;
    using bool_t = std::conditional_t<std::is_const_v<T>, VkBool32 const, VkBool32>;
    using byte_t = std::conditional_t<std::is_const_v<T>, std::byte const, std::byte>;

    constexpr size_t count = feature_flags<feature_t>::COUNT;

    auto* const begin = reinterpret_cast<byte_t*>(&features) + feature_flags<feature_t>::OFFSET;
    return std::span<bool_t, count>{reinterpret_cast<bool_t*>(begin), count};
}
}

namespace cvk::utils {
template<class T>
constexpr bool PhysicalDeviceFeatures::linked() const {
    static_assert(dev::chain_index<T> < STRUCTS, "T must be in the chain");
    return _linked[dev::chain_index<T>];
}
}

CTH_FORMAT_CLASS(
    cvk::utils::MissingFeature,
    "{}, flag: {}",
    [](cvk::utils::MissingFeature const& feature) { return std::tie(feature.sType, feature.index); }
);
