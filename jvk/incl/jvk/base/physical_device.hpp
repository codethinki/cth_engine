#pragma once
#include "jvk/base/queue/queue_family.hpp"
#include "jvk/utility/types.hpp"
#include "jvk/utility/vk_exceptions.hpp"
#include "jvk/utility/device/physical_device_features.hpp"

#include <volk.h>

#include <memory>
#include <span>
#include <string>
#include <vector>



namespace jvk {
class Queue;
class Instance;
class Surface;

class PhysicalDevice {
public:
    using queue_family_map_t = std::unordered_map<queue_family_index_t, QueueFamily>;

    struct State;
    struct CreateResult;


    /**
     * @brief base constructor
     * @param instance @ref Instance::created() required
     */
    explicit PhysicalDevice(
        Instance const& instance,
        utils::PhysicalDeviceFeatures required_features,
        std::span<std::string const> required_extensions
    );

    /**
     * @brief constructs and creates
     * @note calls @ref PhysicalDevice(Instance*, utils::PhysicalDeviceFeatures, std::span<std::string const>, Surface const&)
     * @note calls @ref create()
     */
    explicit PhysicalDevice(
        Instance const& instance,
        utils::PhysicalDeviceFeatures const& required_features,
        std::span<std::string const> required_extensions,
        std::span<Surface const> surfaces,
        jvk::vk_not_null<VkPhysicalDevice> vk_device
    );

    /**
     * @brief constructs and wraps
     * @note calls @ref PhysicalDevice(Instance*, utils::PhysicalDeviceFeatures, std::span<std::string const>, Surface const&)
     * @note calls @ref wrap(State const&)
     */
    explicit PhysicalDevice(
        Instance const& instance,
        utils::PhysicalDeviceFeatures const& required_features,
        std::span<std::string const> required_extensions,
        State const& state
    );



    /**
     * @brief creates if requirements are met
     * @param queue_properties passed to @ref suitable()
     * @return if @ref suitable() returns instance and queue allocation, else nullopt
     */
    static std::optional<CreateResult> Create(
        Instance const& instance,
        std::span<Surface const> surface,
        std::span<QueueFamilyProperties const> queue_properties,
        std::span<std::string const> required_extensions,
        utils::PhysicalDeviceFeatures const& required_features,
        jvk::vk_not_null<VkPhysicalDevice> vk_device
    );

    ~PhysicalDevice() = default;


    /**
     * wraps the @ref State
     * @note resets if @ref created()
     */
    void wrap(State const& state);


    /**
     * creates the physical device
     * @param surfaces to evaluate queue families from
     * @note calls @ref QueueFamily
     */
    void create(std::span<Surface const> surfaces, vk_not_null<VkPhysicalDevice> vk_device);

    /**
     * creates the physical device
     * @note calls @ref getExtensions()
     * @note calls @ref getProperties()
     * @note calls @ref getMemoryProperties()
     * @note calls @ref getQueueFamilyProperties()
     */
    void create(std::span<QueueFamily const> queue_families, vk_not_null<VkPhysicalDevice> vk_device);

    /**
     * @brief enumerates all available devices and picks one that fits the requirements
     * @param queue_properties required queues to support
     * @return physical device & queue family allocation if a suitable one is found, else nullopt
     * @throws cth::except::default_exception if no device is found
     * @note engine required features and extensions are added to the requirements
        - @ref jvk::constants::REQUIRED_DEVICE_FEATURES
        - @ref jvk::constants::REQUIRED_DEVICE_EXTENSIONS
     * @details a single queue set has length N (number of queues), multiple queue sets may be provided.
     */
    [[nodiscard]] static std::optional<CreateResult> AutoPick(
        Instance const& instance,
        std::span<Surface const> temp_surfaces,
        std::span<QueueFamilyProperties const> queue_properties,
        std::span<std::string const> required_extensions = {},
        utils::PhysicalDeviceFeatures const& required_features = {}
    );


    /**
     * @brief evaluates if the device has minimal support
     */
    [[nodiscard]] bool suitable(std::span<QueueFamilyProperties const> queues);



    /**
     * @return indices of missing features from utils::deviceFeaturesToArray
     */
    [[nodiscard]] std::vector<std::variant<size_t, VkStructureType>> supports(
        utils::PhysicalDeviceFeatures const& required_features
    ) const;

    /**
     * @return missing extensions 
     */
    [[nodiscard]] std::vector<std::string> supports(std::span<std::string const> required_extensions);

    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags mem_properties) const;

    [[nodiscard]] VkFormat findSupportedFormat(
        std::span<VkFormat const> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;

    /**
     * @brief finds a combination of queue families that support the requested queue types
     * @param queues_properties requested queue types
     * @pre @ref created()
     * @return order of queue types preserved
     * @return empty if no combination is possible
     */
    [[nodiscard]] std::vector<queue_family_index_t> queueFamilyIndices(
        std::span<QueueFamilyProperties const> queues_properties
    ) const;



    [[nodiscard]] bool supportsQueueSet(std::span<QueueFamilyProperties const> queue_properties) const;

    /**
   * @brief enumerates all available physical devices
   * @throws jvk::result_exception result of @ref vkEnumeratePhysicalDevices()
   */
    [[nodiscard]] static std::vector<VkPhysicalDevice> enumerateVkDevices(
        jvk::vk_not_null<VkInstance> vk_instance
    );

    /**
     * @throws jvk::result_exception result of @ref vkGetPhysicalDeviceProperties()
     */
    [[nodiscard]] static std::vector<std::string> queryExtensions(
        jvk::vk_not_null<VkPhysicalDevice> vk_device
    );

    [[nodiscard]] static VkPhysicalDeviceProperties queryProperties(
        jvk::vk_not_null<VkPhysicalDevice> vk_device
    );

    [[nodiscard]] static VkPhysicalDeviceMemoryProperties queryMemoryProperties(
        jvk::vk_not_null<VkPhysicalDevice> vk_device
    );

    [[nodiscard]] static std::vector<QueueFamily> queryQueueFamilies(
        Surface const& surface,
        jvk::vk_not_null<VkPhysicalDevice> vk_device
    );

    [[nodiscard]] static std::vector<QueueFamily> queryQueueFamilies(
        std::span<Surface const> surfaces,
        vk_not_null<VkPhysicalDevice> vk_device
    );

    [[nodiscard]] static VkSampleCountFlagBits evalMaxSampleCount(
        VkPhysicalDeviceProperties const& properties
    );

private:
    Instance const* _instance;
    utils::PhysicalDeviceFeatures _requiredFeatures;
    std::vector<std::string> _requiredExtensions{};

    move_ptr<VkPhysicalDevice_T> _handle;

    utils::PhysicalDeviceFeatures _features{};
    std::vector<std::string> _extensions{};
    VkPhysicalDeviceProperties _properties{};
    VkSampleCountFlagBits _maxSampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

    VkPhysicalDeviceMemoryProperties _memProperties{};

    queue_family_map_t _queueFamilies{};

public:
    [[nodiscard]] bool created() const { return _handle != nullptr; }

    [[nodiscard]] VkPhysicalDevice get() const { return _handle.get(); }
    [[nodiscard]] auto const& features() const { return _features; }
    [[nodiscard]] auto const& requiredFeatures() const { return _requiredFeatures; }
    [[nodiscard]] std::span<std::string const> extensions() const { return _extensions; }
    [[nodiscard]] std::span<std::string const> requiredExtensions() const { return _requiredExtensions; }
    [[nodiscard]] auto const& properties() const { return _properties; }
    [[nodiscard]] auto const& memProperties() const { return _memProperties; }
    [[nodiscard]] VkSampleCountFlagBits maxSampleCount() const { return _maxSampleCount; }
    [[nodiscard]] VkPhysicalDeviceLimits const& limits() const { return _properties.limits; }
    [[nodiscard]] bool queueFamilyAvailable(queue_family_index_t idx) const { return _queueFamilies.contains(idx); }
    /**
     * @pre @ref queueFamilyAvailable(queue_family_index_t)
     * @param idx queue family index
     */
    [[nodiscard]] QueueFamily const& queueFamily(queue_family_index_t idx) const {
        CTH_CRITICAL(!_queueFamilies.contains(idx), "queue family doesn't exist or is unavailable") {}
        return _queueFamilies.at(idx);
    }

    [[nodiscard]] queue_family_map_t const& queueFamilies() const { return _queueFamilies; }


    PhysicalDevice(PhysicalDevice const& other) = delete;
    PhysicalDevice(PhysicalDevice&& other) noexcept = default;
    PhysicalDevice& operator=(PhysicalDevice const& other) = delete;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = default;

    static void debug_check(PhysicalDevice const&);
    static void debug_check_handle(jvk::vk_not_null<VkPhysicalDevice>);
};
}


namespace jvk {

struct PhysicalDevice::State {
    jvk::vk_not_null<VkPhysicalDevice> vkDevice;
    /**
     * @brief must not be empty
     * @note query with @ref Surface::getQueueFamilies()
     */
    std::vector<QueueFamily> queueFamilies;


    utils::PhysicalDeviceFeatures features{};
    std::vector<std::string> extensions{};
    std::optional<VkPhysicalDeviceProperties> properties = std::nullopt;
    VkSampleCountFlagBits maxSampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    std::optional<VkPhysicalDeviceMemoryProperties> memProperties = std::nullopt;
};
}

namespace jvk {
struct PhysicalDevice::CreateResult {
    jvk::PhysicalDevice device;
    std::vector<queue_family_index_t> queueFamilyIndices;
};
}

//debug checks

namespace jvk {
inline void PhysicalDevice::debug_check(PhysicalDevice const& device) {
    CTH_CRITICAL(!device.created(), "physical device must be created") {}
    debug_check_handle(device.get());
}

inline void PhysicalDevice::debug_check_handle([[maybe_unused]] jvk::vk_not_null<VkPhysicalDevice>) {}

}
