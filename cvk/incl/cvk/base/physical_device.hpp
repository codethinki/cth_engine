#pragma once
#include "cvk/base/queue/queue_family.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/device/physical_device_features.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>


namespace cvk {
class Instance;
class Surface;

/**
 * @brief VkPhysicalDevice wrapper with the queried properties
 * @details copyable, the instance owns the physical device
 */
class PhysicalDevice {
public:
    using vk_handle_t = VkPhysicalDevice;
    struct State;
    struct CreateResult;

    /**
     * @brief base constructor
     * @pre @ref Instance::created()
     */
    explicit PhysicalDevice(
        Instance const&,
        utils::PhysicalDeviceFeatures requiredFeatures,
        std::span<std::string const> requiredExtensions
    );

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref PhysicalDevice(Instance const&, utils::PhysicalDeviceFeatures, std::span<std::string const>)
     * - @ref create(std::span<Surface const>, vk_not_null<VkPhysicalDevice>)
     */
    explicit PhysicalDevice(
        Instance const&,
        utils::PhysicalDeviceFeatures const& requiredFeatures,
        std::span<std::string const> requiredExtensions,
        std::span<Surface const> surfaces,
        vk_not_null<VkPhysicalDevice> vkDevice
    );

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref PhysicalDevice(Instance const&, utils::PhysicalDeviceFeatures, std::span<std::string const>)
     * - @ref wrap()
     */
    explicit PhysicalDevice(
        Instance const&,
        utils::PhysicalDeviceFeatures const& requiredFeatures,
        std::span<std::string const> requiredExtensions,
        State const&
    );

    ~PhysicalDevice() = default;

    /**
     * @brief constructs and creates if the device meets the requirements
     * @param queueProperties passed to @ref suitable()
     * @return std::nullopt -> not @ref suitable()
     */
    [[nodiscard]] static std::optional<CreateResult> Create(
        Instance const&,
        std::span<Surface const> surfaces,
        std::span<QueueFamilyProperties const> queueProperties,
        std::span<std::string const> requiredExtensions,
        utils::PhysicalDeviceFeatures const& requiredFeatures,
        vk_not_null<VkPhysicalDevice> vkDevice
    );

    /**
     * @brief wraps the state
     * @details empty or std::nullopt members of the state -> queries them
     * @post @ref created()
     */
    void wrap(State const&);

    /**
     * @brief queries the device
     * @param surfaces to query the present support of the queue families, empty -> no present support
     * @post @ref created()
     */
    void create(std::span<Surface const> surfaces, vk_not_null<VkPhysicalDevice> vkDevice);

    /**
     * @brief queries the device
     * @param queueFamilies of the device
     * @post @ref created()
     */
    void create(std::span<QueueFamily const> queueFamilies, vk_not_null<VkPhysicalDevice> vkDevice);

    /**
     * @brief enumerates the devices and picks the first that meets the requirements
     * @param queueProperties queues the device must support
     * @return std::nullopt -> no device meets the requirements
     * @details adds to the requirements:
     * - @ref constants::REQUIRED_DEVICE_EXTENSIONS and @ref constants::required_device_features()
     * - a PRESENT queue -> @ref constants::PRESENT_DEVICE_EXTENSIONS
     */
    [[nodiscard]] static std::optional<CreateResult> AutoPick(
        Instance const&,
        std::span<Surface const> surfaces,
        std::span<QueueFamilyProperties const> queueProperties,
        std::span<std::string const> requiredExtensions = {},
        utils::PhysicalDeviceFeatures const& requiredFeatures = {}
    );

    /**
     * @return the device has the required features, the required extensions and the queues
     * @pre @ref created()
     */
    [[nodiscard]] bool suitable(std::span<QueueFamilyProperties const> queueProperties) const;

    /**
     * @return required features the device does not have
     * @pre @ref created()
     */
    [[nodiscard]] std::vector<utils::MissingFeature> supports(
        utils::PhysicalDeviceFeatures const& requiredFeatures
    ) const;

    /**
     * @return required extensions the device does not have
     */
    [[nodiscard]] std::vector<std::string> supports(std::span<std::string const> requiredExtensions) const;

    /**
     * @return first of @ref candidates that supports @ref features with @ref tiling
     * @pre @ref created()
     * @throws cvk::cvk_exception reason: no candidate supports the features
     */
    [[nodiscard]] VkFormat findSupportedFormat(
        std::span<VkFormat const> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;

    /**
     * @brief finds queue families for the requested queues
     * @param queueProperties requested queues
     * @return family index per requested queue, empty -> no combination
     * @pre @ref created()
     */
    [[nodiscard]] std::vector<queue_family_index_t> queueFamilyIndices(
        std::span<QueueFamilyProperties const> queueProperties
    ) const;

    /**
     * @pre @ref created()
     */
    [[nodiscard]] bool supportsQueueSet(std::span<QueueFamilyProperties const> queueProperties) const;

    /**
     * @throws cvk::vk_result_exception result of vkEnumeratePhysicalDevices()
     */
    [[nodiscard]] static std::vector<VkPhysicalDevice> enumerateVkDevices(vk_not_null<VkInstance>);

    /**
     * @throws cvk::vk_result_exception result of vkEnumerateDeviceExtensionProperties()
     */
    [[nodiscard]] static std::vector<std::string> queryExtensions(vk_not_null<VkPhysicalDevice> vkDevice);

    [[nodiscard]] static VkPhysicalDeviceProperties queryProperties(vk_not_null<VkPhysicalDevice> vkDevice);

    [[nodiscard]] static VkPhysicalDeviceMemoryProperties queryMemoryProperties(
        vk_not_null<VkPhysicalDevice> vkDevice
    );

    /**
     * @param surfaces a family presents only if it presents to every surface, empty -> no present support
     */
    [[nodiscard]] static std::vector<QueueFamily> queryQueueFamilies(
        std::span<Surface const> surfaces,
        vk_not_null<VkPhysicalDevice> vkDevice
    );

    [[nodiscard]] static VkSampleCountFlagBits evalMaxSampleCount(VkPhysicalDeviceProperties const& properties);

    [[nodiscard]] constexpr bool created() const { return _handle != nullptr; }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr auto const& features() const { return _features; }
    [[nodiscard]] constexpr auto const& requiredFeatures() const { return _requiredFeatures; }
    [[nodiscard]] constexpr std::span<std::string const> extensions() const { return _extensions; }
    [[nodiscard]] constexpr std::span<std::string const> requiredExtensions() const { return _requiredExtensions; }
    [[nodiscard]] constexpr auto const& properties() const { return _properties; }
    [[nodiscard]] constexpr auto const& memProperties() const { return _memProperties; }
    [[nodiscard]] constexpr VkSampleCountFlagBits maxSampleCount() const { return _maxSampleCount; }
    [[nodiscard]] constexpr VkPhysicalDeviceLimits const& limits() const { return _properties.limits; }

    /**
     * @param idx queue family index
     * @return nullptr -> no family with @ref idx
     */
    [[nodiscard]] QueueFamily const* queueFamily(queue_family_index_t idx) const;

    [[nodiscard]] constexpr std::span<QueueFamily const> queueFamilies() const { return _queueFamilies; }

private:
    not_null_ptr<Instance const> _instance;
    utils::PhysicalDeviceFeatures _requiredFeatures;
    std::vector<std::string> _requiredExtensions{};

    move_ptr<VkPhysicalDevice_T> _handle;

    utils::PhysicalDeviceFeatures _features{};
    std::vector<std::string> _extensions{};
    VkPhysicalDeviceProperties _properties{};
    VkSampleCountFlagBits _maxSampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

    VkPhysicalDeviceMemoryProperties _memProperties{};

    std::vector<QueueFamily> _queueFamilies{};

public:
    PhysicalDevice(PhysicalDevice const& other) = default;
    PhysicalDevice(PhysicalDevice&& other) noexcept = default;
    PhysicalDevice& operator=(PhysicalDevice const& other) = default;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = default;

    static constexpr void debug_check(PhysicalDevice const& device);
    static constexpr void debug_check_handle(vk_not_null<VkPhysicalDevice> vkDevice);
};
}

namespace cvk {
struct PhysicalDevice::State {
    vk_not_null<VkPhysicalDevice> vkDevice;
    /**
     * @details empty -> queried without present support
     */
    std::vector<QueueFamily> queueFamilies;

    std::optional<utils::PhysicalDeviceFeatures> features = std::nullopt;
    std::vector<std::string> extensions{};
    std::optional<VkPhysicalDeviceProperties> properties = std::nullopt;
    VkSampleCountFlagBits maxSampleCount = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    std::optional<VkPhysicalDeviceMemoryProperties> memProperties = std::nullopt;
};

struct PhysicalDevice::CreateResult {
    PhysicalDevice device;
    std::vector<queue_family_index_t> queueFamilyIndices;
};
}

namespace cvk {
constexpr void PhysicalDevice::debug_check(PhysicalDevice const& device) {
    CTH_CRITICAL(!device.created(), "physical device must be created") {}
    debug_check_handle(device.get());
}

constexpr void PhysicalDevice::debug_check_handle([[maybe_unused]] vk_not_null<VkPhysicalDevice> vkDevice) {}
}
