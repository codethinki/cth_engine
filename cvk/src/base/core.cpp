#include "cvk/base/core.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <format>
#include <ranges>
#include <string>


namespace cvk {
using std::span;
using std::vector;

namespace {
    /**
     * @return the uuid as hex in the 8-4-4-4-12 form
     */
    [[nodiscard]] std::string uuid_string(device_uuid_t const& uuid) {
        std::string result{};
        for(auto const [index, byte] : std::views::enumerate(uuid)) {
            if(index == 4 || index == 6 || index == 8 || index == 10) result += '-';
            result += std::format("{:02x}", byte);
        }
        return result;
    }
}

Core::Core(Instance const& instance) : _instance{&instance} {}

Core::Core(Instance const& instance, span<Surface const> surfaces, Config const& config) : Core{instance} {
    create(surfaces, config);
}

Core::Core(Instance const& instance, State state) : Core{instance} { wrap(std::move(state)); }

Core::~Core() { optDestroy(); }

void Core::wrap(State state) {
    PhysicalDevice::debug_check(*state.physicalDevice);
    Device::debug_check(*state.device);
    CTH_CRITICAL(
        &state.device->physicalDevice() != state.physicalDevice.get(),
        "device must be created with the physical device of the state"
    ) {}
    if(state.allocator != nullptr) {
        Allocator::debug_check(*state.allocator);
        CTH_CRITICAL(
            &state.allocator->device() != state.device.get(),
            "allocator must be created with the device of the state"
        ) {}
    }
    CTH_CRITICAL(
        std::ranges::any_of(state.queueIndices, [&](size_t index) { return index >= state.device->queues().size(); }),
        "queue indices must be < queue count of the device"
    ) {}

    optDestroy();

    _physicalDevice = state.physicalDevice.release_val();
    _device = state.device.release_val();
    _allocator = std::move(state.allocator);
    if(_allocator == nullptr) _allocator = std::make_unique<Allocator>(*_device, cvk::create);
    _queueSetIndex = state.queueSetIndex;

    if(state.queueIndices.empty()) _queueIndices.assign_range(std::views::iota(0uz, _device->queues().size()));
    else _queueIndices = std::move(state.queueIndices);
}

void Core::create(span<Surface const> surfaces, Config const& config) {
    Config::debug_check(config);

    optDestroy();

    // the members change after the last step that can throw, a throw leaves the core not created
    auto required = requirements(config);
    auto pick = pickPhysicalDevice(surfaces, config, required);
    auto device = std::make_unique<Device>(
        *_instance,
        *pick.physicalDevice,
        Device::Config{
            .queueFamilyIndices = std::move(pick.uniqueQueueFamilyIndices),
            .features = required.features,
            .extensions = std::move(required.extensions),
        }
    );
    auto allocator = std::make_unique<Allocator>(*device, cvk::create);

    _physicalDevice = std::move(pick.physicalDevice);
    _device = std::move(device);
    _allocator = std::move(allocator);
    _queueSetIndex = pick.queueSetIndex;
    _queueIndices = std::move(pick.queuesToUniqueQueues);
}

void Core::destroy() {
    debug_check(*this);
    forget();
}

Core::State Core::release() {
    debug_check(*this);

    State state{
        .physicalDevice = std::move(_physicalDevice),
        .device = std::move(_device),
        .allocator = std::move(_allocator),
        .queueIndices = std::move(_queueIndices),
        .queueSetIndex = _queueSetIndex,
    };
    forget();
    return state;
}

size_t Core::queueSetIndex() const {
    debug_check(*this);
    CTH_CRITICAL(!_queueSetIndex.has_value(), "queue set index has no value, the config had no queue sets") {}
    return *_queueSetIndex;
}

VkInstance Core::vkInstance() const { return _instance->get(); }

PhysicalDevice const& Core::physicalDevice() const {
    debug_check(*this);
    return *_physicalDevice;
}

VkPhysicalDevice Core::vkPhysicalDevice() const { return physicalDevice().get(); }

Device const& Core::device() const {
    debug_check(*this);
    return *_device;
}

DeviceTable Core::deviceTable() const { return device().table(); }
VolkDeviceTable const* Core::functions() const { return device().functions(); }
VkDevice Core::vkDevice() const { return device().get(); }

Allocator const& Core::allocator() const {
    debug_check(*this);
    return *_allocator;
}

Queue const& Core::queue(size_t idx) const { return device().queues()[queueIndex(idx)]; }

void Core::forget() {
    _queueIndices.clear();
    _allocator = nullptr;
    _device = nullptr;
    _physicalDevice = nullptr;
    _queueSetIndex = std::nullopt;
}

auto Core::requirements(Config const& config) -> device_requirements {
    device_requirements required{.features = constants::BASE_DEVICE_FEATURES, .extensions = config.extensions};
    required.features.merge(config.features);

    bool const present = std::ranges::any_of(
        config.queueProperties,
        [](QueueFamilyProperties properties) { return contains(properties, QueueFamilyProperties::PRESENT); }
    );
    if(!present) return required;

    required.features.merge(constants::PRESENT_DEVICE_FEATURES);
    for(auto const extension : constants::PRESENT_DEVICE_EXTENSIONS)
        if(!std::ranges::contains(required.extensions, extension)) required.extensions.emplace_back(extension);

    return required;
}

auto Core::pickPhysicalDevice(
    span<Surface const> surfaces,
    Config const& config,
    device_requirements const& required
) const -> device_pick {
    if(config.queueSets.empty()) {
        auto pick = tryPickPhysicalDevice(surfaces, config, required, queue_set_t{config.queueProperties.size()});
        CVK_STABLE_THROW(
            !pick && config.device.has_value(),
            "physical device {} is missing or does not support the queues",
            uuid_string(*config.device)
        ) {}
        CVK_STABLE_THROW(!pick, "no physical device supports the queues") {}
        return *std::move(pick);
    }

    for(auto const& [setIndex, queueSet] : std::views::enumerate(config.queueSets)) {
        auto pick = tryPickPhysicalDevice(surfaces, config, required, queueSet);
        if(!pick) continue;

        pick->queueSetIndex = static_cast<size_t>(setIndex);
        return *std::move(pick);
    }

    CVK_STABLE_THROW(
        config.device.has_value(),
        "physical device {} is missing or does not support any queue set",
        uuid_string(*config.device)
    ) {}
    CVK_STABLE_THROW(true, "no physical device supports any queue set") {}
    std::unreachable();
}

auto Core::createQueueSetRelations(
    queue_set_t const& queueSet,
    span<QueueFamilyProperties const> queueProperties
) -> queue_set_relations {
    using index_type = queue_set_t::index_type;

    auto const roots = queueSet.roots();

    vector<size_t> rootToQueue(queueSet.size());
    for(auto const [root, queue] : std::views::zip(roots, std::views::iota(0uz)))
        rootToQueue[root] = queue;

    vector<QueueFamilyProperties> uniqueQueues(roots.size());
    vector<size_t> queueToUniqueQueue(queueProperties.size());

    for(index_type i = 0; i < queueSet.size(); i++) {
        auto const queueIdx = rootToQueue[queueSet.find(i)];
        queueToUniqueQueue[i] = queueIdx;
        uniqueQueues[queueIdx] |= queueProperties[i];
    }

    return {std::move(uniqueQueues), std::move(queueToUniqueQueue)};
}

auto Core::tryPickPhysicalDevice(
    span<Surface const> surfaces,
    Config const& config,
    device_requirements const& required,
    queue_set_t const& queueSet
) const -> std::optional<device_pick> {
    CTH_CRITICAL(queueSet.size() != config.queueProperties.size(), "queue set must span all queues") {}

    auto [uniqueQueues, queueToUniqueQueue] = createQueueSetRelations(queueSet, config.queueProperties);

    auto result = pick_physical_device(
        *_instance,
        surfaces,
        uniqueQueues,
        required.features,
        required.extensions,
        config.device
    );
    if(!result) return std::nullopt;

    return device_pick{
        .physicalDevice = std::make_unique<PhysicalDevice>(std::move(result->device)),
        .uniqueQueueFamilyIndices = std::move(result->queueFamilyIndices),
        .queuesToUniqueQueues = std::move(queueToUniqueQueue),
    };
}

Core::Core(Core&& other) noexcept = default;
Core& Core::operator=(Core&& other) noexcept = default;
}
