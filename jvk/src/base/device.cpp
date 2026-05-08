#include "jvk/base/device.hpp"


#include "jvk/base/core.hpp"
#include "jvk/base/instance.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/base/queue/queue.hpp"
#include "jvk/utility/vk_exceptions.hpp"


namespace jvk {
using std::vector;
using std::string_view;
using std::span;

Device::Device(Instance const& instance, PhysicalDevice const& physical_device) : _instance{&instance},
    _physicalDevice{&physical_device} {}

Device::Device(Instance const& instance, PhysicalDevice const& physical_device, State state) : Device{
    instance,
    physical_device
} { wrap(std::move(state)); }

Device::Device(
    Instance const& instance,
    PhysicalDevice const& physical_device,
    Config const& config
) : Device{
    instance,
    physical_device
} { create(config); }

Device::~Device() { optDestroy(); }

void Device::wrap(State state) {
    optDestroy();

    _handle = state.vkDevice.get();

    _functionTable = std::move(state.functionTable);
    if(_functionTable == nullptr)
        loadFunctionTable();
}

void Device::create(DeviceConfig const& config) {
    optDestroy();

    auto const& familyIndices = config.queueFamilyIndices;
    auto const queueCounts = calcQueueFamiliesQueueCounts(familyIndices);

    createLogicalDevice(queueCounts);
    loadFunctionTable();

    createQueues(familyIndices);
}

void Device::destroy() {
    Device::debug_check(*this);

    destroy(_handle.release(), functions()->vkDestroyDevice);
    reset();
}


auto Device::calcQueueFamiliesQueueCounts(
    std::span<uint32_t const> family_indices
) -> queue_families_queue_counts_t {
    queue_families_queue_counts_t queueCounts{};

    for(auto const& familyIndex : family_indices)
        ++queueCounts[familyIndex];

    return queueCounts;
}

void Device::createLogicalDevice(std::unordered_map<uint32_t, uint32_t> const& queue_families_queue_counts) {
    CTH_CRITICAL(queue_families_queue_counts.empty(), "queue family queue count must be queried first") {}
    CTH_CRITICAL(created(), "device was already created") {}

    vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    queueCreateInfos.reserve(queue_families_queue_counts.size());

    std::vector<std::vector<float>> queuePriorities{};

    for(auto const [queueFamily, queueCount] : queue_families_queue_counts) {
        queuePriorities.emplace_back(queueCount, 1.0f);

        queueCreateInfos.push_back(
            {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .queueFamilyIndex = queueFamily,
                .queueCount = queueCount,
                .pQueuePriorities = queuePriorities.back().data()
            }
        );
    }
    auto const requiredExtensions = cth::str::to_c_str_vector(_physicalDevice->requiredExtensions());


    VkDeviceCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = _physicalDevice->requiredFeatures().get(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(constants::REQUIRED_DEVICE_EXTENSIONS.size()),
        .ppEnabledExtensionNames = requiredExtensions.data(),
    };
    VkDevice ptr = VK_NULL_HANDLE;

    VkResult const createResult = vkCreateDevice(_physicalDevice->get(), &createInfo, nullptr, &ptr);
    JVK_RESULT_STABLE_THROW(createResult != VK_SUCCESS, createResult, "failed to create logical device") {
        reset();
    }
    _handle = ptr;
}

void Device::loadFunctionTable() const { volkLoadDeviceTable(_functionTable.get(), get()); }

void Device::createQueues(span<queue_family_index_t const> family_indices) {
    std::unordered_map<uint32_t, uint32_t> queueCounts{};
    for(auto& index : family_indices)
        queueCounts[index] = 0;

    _queues.reserve(family_indices.size());

    for(auto familyIndex : family_indices) {
        VkQueue ptr = VK_NULL_HANDLE;
        table()->vkGetDeviceQueue(_handle.get(), familyIndex, queueCounts[familyIndex], &ptr);

        JVK_STABLE_THROW(ptr == VK_NULL_HANDLE, "failed to get device queue") {}

        _queues.emplace_back(
            _physicalDevice->queueFamily(familyIndex)->properties,
            Queue::State{this, ptr, familyIndex, queueCounts[familyIndex]++, std::make_shared<std::mutex>()}
        );
    }
}

Device::State Device::release() {
    debug_check(*this);

    State state{
        .vkDevice = _handle.release(),
        .functionTable = std::move(_functionTable)
    };
    reset();
    return state;
}

void Device::waitIdle() const {
    debug_check(*this);

    auto const result = table()->vkDeviceWaitIdle(_handle.get());

    JVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to wait for device") {}
}

void Device::destroy(VkDevice vk_device, PFN_vkDestroyDevice destroy) {
    CTH_CRITICAL(vk_device == VK_NULL_HANDLE, "vk_device must not be invalid (VK_NULL_HANDLE)") {}

    destroy(vk_device, nullptr);

    cth::log::msg<except::LOG>("destroyed vk_device");
}

void Device::reset() {
    _handle = nullptr;

    std::memset(_functionTable.get(), 0, sizeof(decltype(*_functionTable)));
}

}
