#pragma once
#include "CthQueueFamily.hpp"
#include "vulkan/utility/cth_constants.hpp"


#include "vulkan/utility/device/PhysicalDeviceFeatures.hpp"
#include "vulkan/utility/utility/cth_vk_types.hpp"

#include<cth/pointers.hpp>
#include <vulkan/vulkan.h>

#include <memory>
#include <span>
#include <string>
#include <vector>



namespace cth::vk {
class Queue;
class Instance;
class Surface;

class PhysicalDevice {
public:
    struct State;



    /**
     * @brief base constructor
     * @param instance @ref Instance::created() required
     */
    explicit PhysicalDevice(cth::not_null<Instance const*> instance, utils::PhysicalDeviceFeatures required_features,
        std::span<std::string const> required_extensions);

    /**
     * @brief constructs and creates
     * @note calls @ref PhysicalDevice(Instance*, utils::PhysicalDeviceFeatures, std::span<std::string const>, Surface const&)
     * @note calls @ref create()
     */
    explicit PhysicalDevice(cth::not_null<Instance const*> instance, utils::PhysicalDeviceFeatures const& required_features,
        std::span<std::string const> required_extensions, Surface const& surface, vk::not_null<VkPhysicalDevice> vk_device);

    /**
     * @brief constructs and wraps
     * @note calls @ref PhysicalDevice(Instance*, utils::PhysicalDeviceFeatures, std::span<std::string const>, Surface const&)
     * @note calls @ref wrap(State const&)
     */
    explicit PhysicalDevice(cth::not_null<Instance const*> instance, utils::PhysicalDeviceFeatures const& required_features,
        std::span<std::string const> required_extensions, State const& state);



    /**
     * @brief creates if requirements are met
     * @return if @ref suitable() with @param queues returns instance, else nullopt
     */
    static std::optional<PhysicalDevice> Create(cth::not_null<Instance const*> instance, Surface const& surface, std::span<Queue const> queues,
        std::span<std::string const> required_extensions, utils::PhysicalDeviceFeatures const& required_features,
        vk::not_null<VkPhysicalDevice> vk_device);

    ~PhysicalDevice() = default;


    /**
     * @brief wraps the @ref State
     * @note resets if @ref created()
     */
    void wrap(State const& state);

    /**
     * @param surface @ref Surface::created() required
     * @note calls @ref getExtensions()
     * @note calls @ref getProperties()
     * @note calls @ref getMemoryProperties()
     * @note calls @ref getQueueFamilyProperties()
     */
    void create(Surface const& surface, cth::not_null<VkPhysicalDevice> vk_device);

    /**
     * @brief enumerates all available devices and picks one that fits the requirements
     * @param queues required queues to support
     * @return valid physical device
     * @throws cth::except::default_exception if no device is found
     * @note engine required features and extensions are added to the requirements
     * @link cth::vk::constants::REQUIRED_DEVICE_FEATURES
     * @link cth::vk::constants::REQUIRED_DEVICE_EXTENSIONS
     */
    [[nodiscard]] static std::unique_ptr<PhysicalDevice> AutoPick(cth::not_null<Instance const*> instance, std::span<Queue const> queues,
        std::span<std::string const> required_extensions, utils::PhysicalDeviceFeatures const& required_features);


    /**
     * @brief evaluates if the device has minimal support
     */
    [[nodiscard]] bool suitable(std::span<Queue const> queues);



    /**
     * @return indices of missing features from utils::deviceFeaturesToArray
     */
    [[nodiscard]] std::vector<std::variant<size_t, VkStructureType>> supports(utils::PhysicalDeviceFeatures const& required_features) const;

    /**
     * @return missing extensions 
     */
    [[nodiscard]] std::vector<std::string> supports(std::span<std::string const> required_extensions);

    [[nodiscard]] uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags mem_properties) const;
    [[nodiscard]] VkFormat findSupportedFormat(std::span<VkFormat const> candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    /**
     * @brief finds a combination of queue families that support the requested queue types
     * @param queues requested queue types
     * @return order of queue types preserved
     * @return empty if no combination is possible
     */
    [[nodiscard]] std::vector<uint32_t> queueFamilyIndices(std::span<Queue const> queues) const;



    [[nodiscard]] bool supportsQueueSet(std::span<Queue const> queues) const;

    /**
   * @brief enumerates all available physical devices
   * @throws cth::vk::result_exception result of @ref vkEnumeratePhysicalDevices()
   */
    [[nodiscard]] static std::vector<VkPhysicalDevice> enumerateVkDevices(vk::not_null<VkInstance> vk_instance);

    /**
     * @throws cth::vk::result_exception result of @ref vkGetPhysicalDeviceProperties()
     */
    static [[nodiscard]] std::vector<std::string> getExtensions(vk::not_null<VkPhysicalDevice> vk_device);

    static [[nodiscard]] VkPhysicalDeviceProperties getProperties(vk::not_null<VkPhysicalDevice> vk_device);

    static [[nodiscard]] VkPhysicalDeviceMemoryProperties getMemoryProperties(vk::not_null<VkPhysicalDevice> vk_device);

    static [[nodiscard]] std::vector<QueueFamily> getQueueFamilies(Surface const& surface, vk::not_null<VkPhysicalDevice> vk_device);

    static [[nodiscard]] VkSampleCountFlagBits evalMaxSampleCount(VkPhysicalDeviceProperties const& properties);

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
    std::vector<QueueFamily> _queueFamilies{};



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


    PhysicalDevice(PhysicalDevice const& other) = delete;
    PhysicalDevice(PhysicalDevice&& other) noexcept = default;
    PhysicalDevice& operator=(PhysicalDevice const& other) = delete;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(cth::not_null<PhysicalDevice const*> device);
    static void debug_check_handle(vk::not_null<VkPhysicalDevice> vk_device);
#define DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device) PhysicalDevice::debug_check_handle(vk_device)
#define DEBUG_CHECK_PHYSICAL_DEVICE(device_ptr) PhysicalDevice::debug_check(device_ptr)
#else
#define DEBUG_CHECK_PHYSICAL_DEVICE_HANDLE(vk_device) ((void)0)
#define DEBUG_CHECK_PHYSICAL_DEVICE(device_ptr) ((void)0)
#endif

};
}

//State

namespace cth::vk {

struct PhysicalDevice::State {
    vk::not_null<VkPhysicalDevice> vkDevice;
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
