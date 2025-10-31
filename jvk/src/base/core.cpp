#include "jvk/base/core.hpp"

#include "jvk/base/device.hpp"
#include "jvk/base/instance.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"
#include "jvk/utility/os/os_window.hpp"

#include <map>


namespace jvk {

Core::Core(State state) { wrap(std::move(state)); }
Core::Core(Config const& config) { create(config); }

Core::~Core() { optDestroy(); }

void Core::wrap(State state) {
    Instance::debug_check(*state.instance);
    PhysicalDevice::debug_check(*state.physicalDevice);
    Device::debug_check(*state.device);
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(state.destructionQueue);

    optDestroy();

    _instance = state.instance.release_val();
    _physicalDevice = state.physicalDevice.release_val();
    _device = state.device.release_val();
    _destructionQueue = std::move(state.destructionQueue);
}

void Core::create(Config const& config) {
    optDestroy();

    _instance = std::make_unique<Instance>(config.appName, config.requiredExtensions, std::nullopt);

    auto uniqueQueues = createPhysicalDevice(config.queues, config.queueSets);

    //TODO very ugly code
    std::vector<Queue> queues{};
    queues.reserve(uniqueQueues.size());
    std::unordered_map<size_t, Queue const*> setIndexToQueue{};

    for(auto const& [index, properties] : uniqueQueues) {
        queues.emplace_back(properties);
        setIndexToQueue[index] = &queues.back();
    }


    _device = std::make_unique<Device>(*_instance, *_physicalDevice, queues);

    wrapQueues(setIndexToQueue, config.queues, config.queueSets[*_queueSetIndex]);

    if(config.destructionQueue) _destructionQueue = std::make_unique<DestructionQueue>();
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
    std::span<Queue const> queues,
    std::span<queue_set_t const> queue_sets
) -> std::map<size_t, QueueFamilyProperties> {
    auto windows = os::create_hidden_monitor_windows();

    JVK_STABLE_THROW(windows.empty(), "failed to create temp monitor windows") {}

    std::vector surfaces{
        std::from_range,
        windows | std::views::transform(
            [this](os::window_t const& window) { return os::surface_from_window(*_instance, window); }
        )
    };

    std::vector queueProperties{
        std::from_range,
        queues | std::views::transform([](Queue const& queue) { return queue.familyProperties(); })
    };


    for(size_t setIndex = 0; setIndex < queue_sets.size(); setIndex++) {
        auto uniqueQueues = tryCreatePhysicalDevice(surfaces, queueProperties, queue_sets[setIndex]);
        if(uniqueQueues.empty()) continue;

        _queueSetIndex = setIndex;

        return uniqueQueues;
    }

    JVK_STABLE_THROW(
        !_physicalDevice,
        "failed to create physical device, probably no suitable queue configuration found"
    ) {}

    return {};
}
std::map<size_t, QueueFamilyProperties> Core::tryCreatePhysicalDevice(
    std::span<Surface const> surfaces,
    std::span<QueueFamilyProperties const> queue_properties,
    queue_set_t queue_set
) {
    CTH_CRITICAL(queue_set.size() != queue_properties.size(), "queue set must span all queues") {}

    using index_type = cth::dt::union_find::index_type;
    std::map<index_type, QueueFamilyProperties> uniqueQueues{};

    for(index_type i = 0; i < queue_set.size(); i++)
        uniqueQueues[queue_set.find(i)] |= queue_properties[i];

    std::vector properties{std::from_range, uniqueQueues | std::views::values};

    // ReSharper disable once CppLocalVariableMayBeConst (device has no copy ctor -> moving requires non-const)
    auto deviceOpt = PhysicalDevice::AutoPick(
        *_instance,
        surfaces,
        properties
    );

    if(!deviceOpt) return {};

    _physicalDevice = std::make_unique<PhysicalDevice>(std::move(*deviceOpt));


    return uniqueQueues;
}
void Core::wrapQueues(
    std::unordered_map<size_t, Queue const*> const& physical_queues,
    std::span<Queue> queues,
    queue_set_t const& queue_set
) const {
    for(size_t i = 0; i < queues.size(); i++) {
        auto const& uniqueQueue = physical_queues.at(queue_set.find(i));
        //BUG this is wrong uniqueQueue has weird parameters
        queues[i].wrap({uniqueQueue->get(), _device.get(), uniqueQueue->familyIndex(), uniqueQueue->index()});
    }
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
