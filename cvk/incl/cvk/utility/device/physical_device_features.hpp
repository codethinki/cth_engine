#pragma once
#include "cvk/utility/format.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/string/format.hpp>

#include <cstddef>
#include <span>
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
 * @brief features of the vulkan 1.0 to 1.3 core as one pNext chain
 * @details copyable, copy and move link the chain again
 */
class PhysicalDeviceFeatures {
public:
    using chain_t = std::tuple<
        VkPhysicalDeviceFeatures2,
        VkPhysicalDeviceVulkan11Features,
        VkPhysicalDeviceVulkan12Features,
        VkPhysicalDeviceVulkan13Features
    >;

    /**
     * @brief every feature false
     */
    PhysicalDeviceFeatures();

    /**
     * @brief queries the features of the device
     */
    explicit PhysicalDeviceFeatures(vk_not_null<VkPhysicalDevice> vkDevice);

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
     * @tparam T one of @ref chain_t
     */
    template<class T = VkPhysicalDeviceFeatures2>
    [[nodiscard]] auto& features(this auto& self) { return std::get<T>(self._chain); }

    /**
     * @brief start of the pNext chain, for VkDeviceCreateInfo::pNext
     */
    [[nodiscard]] VkPhysicalDeviceFeatures2 const* get() const { return &std::get<VkPhysicalDeviceFeatures2>(_chain); }

private:
    void link();

    chain_t _chain;

public:
    PhysicalDeviceFeatures(PhysicalDeviceFeatures const& other) : _chain{other._chain} { link(); }
    PhysicalDeviceFeatures(PhysicalDeviceFeatures&& other) noexcept : _chain{other._chain} { link(); }
    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures const& other) {
        _chain = other._chain;
        link();
        return *this;
    }
    PhysicalDeviceFeatures& operator=(PhysicalDeviceFeatures&& other) noexcept {
        _chain = other._chain;
        link();
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

static_assert(feature_flags<VkPhysicalDeviceFeatures2>::COUNT == 55);
static_assert(feature_flags<VkPhysicalDeviceVulkan11Features>::COUNT == 12);
static_assert(feature_flags<VkPhysicalDeviceVulkan12Features>::COUNT == 47);
static_assert(feature_flags<VkPhysicalDeviceVulkan13Features>::COUNT == 15);

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

CTH_FORMAT_CLASS(
    cvk::utils::MissingFeature,
    "{}, flag: {}",
    [](cvk::utils::MissingFeature const& feature) { return std::tie(feature.sType, feature.index); }
);
