#include "jvk/base/core.hpp"

#include "jvk/base/destruction_queue.hpp"
#include "jvk/base/device.hpp"
#include "jvk/base/instance.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/utility/vk_exceptions.hpp"
#include "jvk/utility/os/os_window.hpp"

#include <map>


namespace jvk {

Core::Core() = default;
Core::Core(State state) { wrap(std::move(state)); }
Core::Core(Config const& config) { create(config); }

Core::~Core() { optDestroy(); }

void Core::wrap(State state) {
    Instance::debug_check(*state.instance);
    PhysicalDevice::debug_check(*state.physicalDevice);
    Device::debug_check(*state.device);
    DestructionQueue::debug_check_null_allowed(state.destructionQueue.get());

    optDestroy();

    _instance = state.instance.release_val();
    _physicalDevice = state.physicalDevice.release_val();
    _device = state.device.release_val();
    _destructionQueue = std::move(state.destructionQueue);
}

void Core::create(Config const& config) {
    Config::debug_check(config);

    optDestroy();

    _instance = std::make_unique<Instance>(config.appName, config.requiredExtensions, std::nullopt);

    auto [uniqueQueueFamilyIndices, queuesToUniqueQueues] = createPhysicalDevice(config.queueProperties, config.queueSets);

    _device = std::make_unique<Device>(
        *_instance,
        *_physicalDevice,
        Device::Config{std::move(uniqueQueueFamilyIndices)}
    );

    createQueues(queuesToUniqueQueues);
    if(config.destructionQueueConfig)
        _destructionQueue = std::make_unique<DestructionQueue>(*config.destructionQueueConfig);
}

void Core::destroy() {
    debug_check(*this);

    _destructionQueue = nullptr;
    _device = nullptr;
    _physicalDevice = nullptr;
    _instance = nullptr;

    reset();
}

void Core::reset() {
    _destructionQueue = nullptr;
    _device = nullptr;
    _physicalDevice = nullptr;
    _queueSetIndex = std::nullopt;
    _instance = nullptr;
}

Core::State Core::release() {
    State temp{
        std::move(_instance),
        std::move(_physicalDevice),
        std::move(_device),
        std::move(_destructionQueue),
    };

    Core::reset();

    return temp;
}
auto Core::createPhysicalDevice(
    std::span<QueueFamilyProperties const> queues,
    std::span<queue_set_t const> queue_sets
) -> queue_mappings {
    auto windows = os::create_hidden_monitor_windows();

    JVK_STABLE_THROW(windows.empty(), "failed to create temp monitor windows") {}

    std::vector surfaces{
        std::from_range,
        windows | std::views::transform(
            [this](os::window_t const& window) { return os::surface_from_window(*_instance, window); }
        )
    };


    for(size_t setIndex = 0; setIndex < queue_sets.size(); setIndex++) {
        auto const uniqueQueues = tryCreatePhysicalDevice(surfaces, queues, queue_sets[setIndex]);
        if(!uniqueQueues)
            continue;

        _queueSetIndex = setIndex;

        return *uniqueQueues;
    }

    JVK_STABLE_THROW(
        !_physicalDevice,
        "failed to create physical device, probably no suitable queue configuration found"
    ) {}

    return {};
}
auto Core::tryCreatePhysicalDevice(
    std::span<Surface const> surfaces,
    std::span<QueueFamilyProperties const> queue_properties,
    queue_set_t const& queue_set
) -> std::optional<queue_mappings> {
    CTH_CRITICAL(queue_set.size() != queue_properties.size(), "queue set must span all queues") {}

    using index_type = cth::dt::union_find::index_type;

    auto const roots = queue_set.roots();

    std::unordered_map<index_type, size_t> rootToQueue{
        std::from_range,
        std::views::zip(roots, std::views::iota(0uz))
    };


    for(size_t i = 0; i < roots.size(); i++)
        rootToQueue[roots[i]] = i;

    std::vector<QueueFamilyProperties> uniqueQueues{roots.size()};
    std::vector queueToUniqueQueue{queue_properties.size()};

    for(index_type i = 0; i < queue_set.size(); i++) {
        auto const queueIdx = rootToQueue[queue_set.find(i)];
        queueToUniqueQueue[i] = queueIdx;
        uniqueQueues[queueIdx] |= queue_properties[i];
    }
    // ReSharper disable once CppLocalVariableMayBeConst (device has no copy ctor -> moving requires non-const)
    auto resultOpt = PhysicalDevice::AutoPick(
        *_instance,
        surfaces,
        uniqueQueues
    );

    if(!resultOpt)
        return {};

    _physicalDevice = std::make_unique<PhysicalDevice>(std::move(resultOpt->device));

    return queue_mappings{resultOpt->queueFamilyIndices, queueToUniqueQueue};
}
void Core::createQueues(std::span<size_t const> queues_to_unique_queues) {
    _queues.reserve(queues_to_unique_queues.size());

    auto uniqueQueues = _device->queues();

    for(auto const uniqueQueueIdx : queues_to_unique_queues)
        _queues.emplace_back(uniqueQueues[uniqueQueueIdx]);
}


Device const& Core::device() const { return *_device; }
DeviceTable Core::deviceTable() const { return _device->table(); }
VolkDeviceTable const* Core::functions() const { return _device->functions(); }
VkDevice Core::vkDevice() const { return _device->get(); }
PhysicalDevice const& Core::physicalDevice() const { return *_physicalDevice; }
VkPhysicalDevice Core::vkPhysicalDevice() const { return _physicalDevice->get(); }
Instance const& Core::instance() const { return *_instance; }
VkInstance Core::vkInstance() const { return _instance->get(); }
DestructionQueue* Core::destructionQueue() const { return _destructionQueue.get(); }

}
