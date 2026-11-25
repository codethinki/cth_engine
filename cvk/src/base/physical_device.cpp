#include "cvk/base/physical_device.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constant/os_constants.hpp"
#include "cvk/utility/device/feature_chain.hpp"
#include "cvk/utility/enumerate.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/algorithm/combine.hpp>
#include <cth/data/string_joiner.hpp>

#include <algorithm>
#include <ranges>


namespace cvk {
using std::span;
using std::string;
using std::vector;

namespace {
    [[nodiscard]] vector<string> query_extensions(vk_not_null<VkPhysicalDevice> vkDevice) {
        auto const properties = dev::enumerate(
            "device extensions",
            vkEnumerateDeviceExtensionProperties,
            vkDevice.get(),
            nullptr
        );

        return {
            std::from_range,
            properties | std::views::transform(
                [](VkExtensionProperties const& extension) {
                    return string{extension.extensionName};
                }
            )
        };
    }

    struct queried_properties {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceDescriptorHeapPropertiesEXT descriptorHeapProperties;
        device_uuid_t uuid;
    };

    /** the query includes the descriptor heap properties only with their extension */
    [[nodiscard]] queried_properties query_properties(
        vk_not_null<VkPhysicalDevice> vkDevice,
        span<string const> extensions
    ) {
        VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT,
            .pNext = nullptr,
        };
        bool const heaps = std::ranges::contains(
            extensions,
            std::string_view{VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME}
        );

        VkPhysicalDeviceIDProperties idProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
            .pNext = heaps ? &heapProperties : nullptr,
        };
        VkPhysicalDeviceProperties2 properties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &idProperties,
        };
        vkGetPhysicalDeviceProperties2(vkDevice.get(), &properties);

        heapProperties.pNext = nullptr;
        queried_properties result{
            .properties = properties.properties,
            .descriptorHeapProperties = heapProperties,
            .uuid = {},
        };
        std::ranges::copy(idProperties.deviceUUID, result.uuid.begin());
        return result;
    }

    [[nodiscard]] VkPhysicalDeviceMemoryProperties query_memory_properties(
        vk_not_null<VkPhysicalDevice> vkDevice
    ) {
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(vkDevice.get(), &properties);
        return properties;
    }

    /**
     * a family presents only if it presents to every surface, empty surfaces -> no present support
     */
    [[nodiscard]] vector<QueueFamily> query_queue_families(
        span<cth::ref_wrapper<Surface const> const> surfaces,
        vk_not_null<VkPhysicalDevice> vkDevice
    ) {
        auto const familyProperties = dev::enumerate(
            "queue families",
            vkGetPhysicalDeviceQueueFamilyProperties,
            vkDevice.get()
        );

        vector<QueueFamily> queueFamilies{};
        queueFamilies.reserve(familyProperties.size());

        for(uint32_t i = 0; i < familyProperties.size(); i++) {
            bool presentSupport = !surfaces.empty();

            for(Surface const& surface : surfaces) {
                VkBool32 support = VK_FALSE;
                auto const result = vkGetPhysicalDeviceSurfaceSupportKHR(
                    vkDevice.get(),
                    i,
                    surface.get(),
                    &support
                );
                CVK_RESULT_STABLE_THROW(
                    result != VK_SUCCESS,
                    result,
                    "device-surface support query failed"
                ) {}

                presentSupport = presentSupport && support == VK_TRUE;
            }

            queueFamilies.push_back(QueueFamily::Vk(i, familyProperties[i], presentSupport));
        }

        return queueFamilies;
    }

    [[nodiscard]] VkSampleCountFlagBits max_sample_count(
        VkPhysicalDeviceProperties const& properties
    ) {
        VkSampleCountFlags const supportedSamples = properties.limits.framebufferColorSampleCounts
            & properties.limits.framebufferDepthSampleCounts;

        constexpr std::array sampleFlagBits = {
            VK_SAMPLE_COUNT_64_BIT,
            VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_16_BIT,
            VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_2_BIT,
            VK_SAMPLE_COUNT_1_BIT
        };

        for(auto const sampleFlagBit : sampleFlagBits)
            if(supportedSamples & sampleFlagBit)
                return sampleFlagBit;

        CTH_CRITICAL(true, "invalid state, sample count not supported") {}
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

PhysicalDevice::PhysicalDevice(
    vk_not_null<VkPhysicalDevice> vkDevice,
    span<cth::ref_wrapper<Surface const> const> surfaces
) : _handle{vkDevice.get()},
    _extensions{query_extensions(vkDevice)},
    _features{dev::query_features(vkDevice, _extensions)},
    _memoryProperties{query_memory_properties(vkDevice)},
    _queueFamilies{query_queue_families(surfaces, vkDevice)} {
    auto const [properties, heapProperties, uuid] = query_properties(vkDevice, _extensions);
    _properties = properties;
    _descriptorHeapProperties = heapProperties;
    _uuid = uuid;
    _maxSampleCount = max_sample_count(_properties);
}

vector<string> PhysicalDevice::missing(
    DeviceFeatures const& features,
    span<string const> extensions
) const {
    debug_check(*this);

    vector<string> missing{};
    for(auto const name : _features.missing(features))
        missing.emplace_back(name);

    auto const add = [&](std::string_view extension) {
        if(!std::ranges::contains(_extensions, extension) && !std::ranges::contains(
            missing,
            extension
        ))
            missing.emplace_back(extension);
    };
    for(auto const& extension : extensions)
        add(extension);
    for(auto const extension : dev::extensions_of(features))
        add(extension);

    return missing;
}

auto PhysicalDevice::queueFamilyIndices(
    span<QueueFamilyProperties const> queueProperties
) const -> vector<queue_family_index_t> {
    debug_check(*this);

    if(queueProperties.empty())
        return {};

    vector<vector<queue_family_index_t>> queueIndices{queueProperties.size()};

    for(auto [requiredProperties, indices] : std::views::zip(queueProperties, queueIndices))
        for(auto const& family : _queueFamilies)
            if((family.properties & requiredProperties) == requiredProperties)
                indices.push_back(family.index);

    vector<size_t> familiesMaxQueues(_queueFamilies.size());

    for(auto [src, dst] : std::views::zip(_queueFamilies, familiesMaxQueues))
        dst = src.vkProperties.queueCount;

    return cth::alg::assign(queueIndices, familiesMaxQueues);
}

auto PhysicalDevice::findSupportedFormat(
    span<VkFormat const> candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const -> VkFormat {
    debug_check(*this);

    for(VkFormat const format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(_handle.get(), format, &props);

        if(tiling == VK_IMAGE_TILING_OPTIMAL
            && (props.optimalTilingFeatures & features) == features)
            return format;
        if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
    }
    CVK_STABLE_THROW(true, "format unsupported") {}
    return VK_FORMAT_MAX_ENUM;
}

QueueFamily const* PhysicalDevice::queueFamily(queue_family_index_t familyIdx) const {
    auto const it = std::ranges::find(_queueFamilies, familyIdx, &QueueFamily::index);

    if(it == _queueFamilies.end())
        return nullptr;

    return &*it;
}

VkExternalFenceFeatureFlags PhysicalDevice::externalFenceFeatures(
    VkExternalFenceHandleTypeFlagBits handleType
) const {
    debug_check(*this);

    VkPhysicalDeviceExternalFenceInfo const info{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
        .pNext = nullptr,
        .handleType = handleType,
    };
    VkExternalFenceProperties properties{
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES,
        .pNext = nullptr
    };
    vkGetPhysicalDeviceExternalFenceProperties(_handle.get(), &info, &properties);

    return properties.externalFenceFeatures;
}

vector<PhysicalDevice> enumerate_physical_devices(
    Instance const& instance,
    span<cth::ref_wrapper<Surface const> const> surfaces
) {
    Instance::debug_check(instance);

    auto const vkDevices = dev::enumerate(
        "physical devices",
        vkEnumeratePhysicalDevices,
        instance.get()
    );

    return {
        std::from_range,
        vkDevices | std::views::transform(
            [surfaces](VkPhysicalDevice vkDevice) { return PhysicalDevice{vkDevice, surfaces}; }
        )
    };
}

std::optional<PhysicalDevicePick> pick_physical_device(
    Instance const& instance,
    span<cth::ref_wrapper<Surface const> const> surfaces,
    span<QueueFamilyProperties const> queueProperties,
    DeviceFeatures const& features,
    span<string const> extensions,
    std::optional<device_uuid_t> const& uuidOpt
) {
    std::optional<PhysicalDevicePick> bestOpt{};

    bool const present = std::ranges::any_of(
        queueProperties,
        [](QueueFamilyProperties properties) {
            return contains(properties, QueueFamilyProperties::PRESENT);
        }
    );

    for(auto& device : enumerate_physical_devices(instance, surfaces)) {
        if(uuidOpt.has_value() && device.uuid() != *uuidOpt)
            continue;

        auto const missing = device.missing(features, extensions);
        auto indices = device.queueFamilyIndices(queueProperties);

        // the present fences are exportable, see Swapchain
        auto const fenceFeatures = device.externalFenceFeatures(constants::OS_FENCE_HANDLE_TYPE);
        bool const osFences = !present
            || (fenceFeatures & constants::OS_FENCE_FEATURES) == constants::OS_FENCE_FEATURES;

        if(missing.empty() && !indices.empty() && osFences) {
            auto const rank = dev::device_type_rank(device.properties().deviceType);
            if(!bestOpt.has_value() || rank < dev::device_type_rank(
                bestOpt->device.properties().deviceType
            ))
                bestOpt = PhysicalDevicePick{std::move(device), std::move(indices)};
            continue;
        }

        cth::dt::string_joiner reasons{", "};
        if(indices.empty())
            reasons += "queue families";
        if(!osFences)
            reasons += "os fence features";
        for(auto const& name : missing)
            reasons += name;
        log::msg<except::LOG>(
            "physical device {} misses: {}",
            device.properties().deviceName,
            std::string{reasons}
        );
    }

    if(bestOpt.has_value())
        log::msg<except::INFO>(
            "chosen physical device: {}",
            bestOpt->device.properties().deviceName
        );
    return bestOpt;
}
}
