#include "cvk/base/device.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/device/feature_chain.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/string.hpp>

#include <algorithm>
#include <cstring>
#include <ranges>


namespace cvk {
using std::span;
using std::vector;

Device::Device(Instance const& instance, PhysicalDevice const& physicalDevice) : _instance{&instance},
    _physicalDevice{&physicalDevice} {}

Device::Device(Instance const& instance, PhysicalDevice const& physicalDevice, State state) : Device{
    instance,
    physicalDevice
} { wrap(std::move(state)); }

Device::Device(
    Instance const& instance,
    PhysicalDevice const& physicalDevice,
    Config const& config
) : Device{
    instance,
    physicalDevice
} { create(config); }

void Device::wrap(State state) {
    optDestroy();

    _functionTable = std::move(state.functionTable);
    if(_functionTable == nullptr) {
        _functionTable = std::make_unique<VolkDeviceTable>();
        loadFunctionTable(state.vkDevice.get());
    }

    _handle.reset(state.vkDevice.get());
    _features = state.features;
    _extensions = std::move(state.extensions);
    createQueues(state.queueFamilyIndices);
}

void Device::create(Config const& config) {
    optDestroy();

    _features = config.features;
    _extensions = config.extensions;
    for(auto const extension : dev::extensions_of(config.features))
        if(!std::ranges::contains(_extensions, extension)) _extensions.emplace_back(extension);

    auto const& familyIndices = config.queueFamilyIndices;
    auto const queueCounts = calcQueueFamiliesQueueCounts(familyIndices);

    createLogicalDevice(queueCounts);

    createQueues(familyIndices);
}

void Device::destroy() {
    debug_check(*this);

    _handle.reset();
    forget();
}

Device::State Device::release() {
    debug_check(*this);

    State state{
        .vkDevice = _handle.release(),
        .functionTable = std::move(_functionTable),
        .queueFamilyIndices = {std::from_range, _queues | std::views::transform(&Queue::familyIndex)},
        .features = _features,
        .extensions = std::move(_extensions),
    };
    forget();
    return state;
}

void Device::waitIdle() const {
    debug_check(*this);

    auto const result = functions()->vkDeviceWaitIdle(_handle.get());
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to wait for device") {}
}

void Device::destroy(VkDevice vkDevice) {
    CTH_WARN(vkDevice == VK_NULL_HANDLE, "vkDevice invalid (VK_NULL_HANDLE)") {}

    auto const destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(vkGetDeviceProcAddr(vkDevice, "vkDestroyDevice"));
    destroyDevice(vkDevice, nullptr);

    log::msg<except::LOG>("destroyed device");
}

void Device::forget() {
    _queues.clear();
    _features = {};
    _extensions.clear();

    if(_functionTable == nullptr) _functionTable = std::make_unique<VolkDeviceTable>();
    std::memset(_functionTable.get(), 0, sizeof(VolkDeviceTable));
}

auto Device::calcQueueFamiliesQueueCounts(span<uint32_t const> familyIndices) -> queue_families_queue_counts_t {
    queue_families_queue_counts_t queueCounts{};

    for(auto const& familyIndex : familyIndices)
        ++queueCounts[familyIndex];

    return queueCounts;
}

void Device::createLogicalDevice(queue_families_queue_counts_t const& queueFamiliesQueueCounts) {
    CTH_CRITICAL(queueFamiliesQueueCounts.empty(), "at least one queue is required") {}
    CTH_CRITICAL(created(), "device was already created") {}

    vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    queueCreateInfos.reserve(queueFamiliesQueueCounts.size());

    vector<vector<float>> queuePriorities{};
    queuePriorities.reserve(queueFamiliesQueueCounts.size());

    for(auto const [queueFamily, queueCount] : queueFamiliesQueueCounts) {
        queuePriorities.emplace_back(queueCount, 1.0f);

        queueCreateInfos.push_back(
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .queueFamilyIndex = queueFamily,
                .queueCount = queueCount,
                .pQueuePriorities = queuePriorities.back().data()
            }
        );
    }
    auto const extensions = cth::str::to_c_str_vector(_extensions);
    dev::FeatureChain const features{_features, _extensions};

    VkDeviceCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = features.get(),
        .flags = 0,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
        .pEnabledFeatures = nullptr,
    };
    VkDevice ptr = VK_NULL_HANDLE;

    auto const createResult = vkCreateDevice(_physicalDevice->get(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create logical device") {
        forget();
        throw cvk::vk_result_exception{createResult, details->exception()};
    }

    loadFunctionTable(ptr);
    _handle.reset(ptr);
}

void Device::loadFunctionTable(VkDevice vkDevice) const { volkLoadDeviceTable(_functionTable.get(), vkDevice); }

void Device::createQueues(span<queue_family_index_t const> familyIndices) {
    std::unordered_map<uint32_t, uint32_t> queueCounts{};

    _queues.reserve(familyIndices.size());

    for(auto const familyIndex : familyIndices) {
        auto& queueIndex = queueCounts[familyIndex];

        VkQueue ptr = VK_NULL_HANDLE;
        table()->vkGetDeviceQueue(_handle.get(), familyIndex, queueIndex, &ptr);

        // vkGetDeviceQueue() has no failure, a null queue means the device has no such queue
        CTH_CRITICAL(ptr == VK_NULL_HANDLE, "the device has no queue {} in family {}", queueIndex, familyIndex) {}

        _queues.emplace_back(
            _physicalDevice->queueFamily(familyIndex)->properties,
            Queue::State{_functionTable.get(), ptr, familyIndex, queueIndex++, std::make_shared<std::mutex>()}
        );
    }
}
}
