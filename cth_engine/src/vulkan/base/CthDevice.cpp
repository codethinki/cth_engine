#include "CthDevice.hpp"


#include "CthCore.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"
#include "queue/CthQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"


namespace cth::vk {
using std::vector;
using std::string_view;
using std::span;

Device::Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device) :
    _instance{instance}, _physicalDevice{physical_device} {}
Device::Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device, State state) :
    Device{instance, physical_device} { wrap(std::move(state)); }
Device::Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device, std::span<Queue> queues) :
    Device{instance, physical_device} { create(queues); }

Device::~Device() { optDestroy(); }

void Device::wrap(State state) {
    optDestroy();

    _handle = state.vkDevice.get();
    _queueFamiliesQueueCounts = state.queueFamiliesQueueCounts;

    _functionTable = std::move(state.functionTable);
    if(_functionTable == nullptr) createFunctionTable();

}
void Device::create(std::span<Queue> queues) {
    optDestroy();

    auto const familyIndices = setUniqueFamilyIndices(queues);

    createLogicalDevice();
    createFunctionTable();

    wrapQueues(familyIndices, queues);
}
void Device::destroy() {
    DEBUG_CHECK_DEVICE(this);

    destroy(_handle.release());
    reset();
}

vector<uint32_t> Device::setUniqueFamilyIndices(span<Queue const> queues) {
    auto const& queueFamilyIndices = _physicalDevice->queueFamilyIndices(queues);

    for(auto const& familyIndex : queueFamilyIndices) ++_queueFamiliesQueueCounts[familyIndex];


    return queueFamilyIndices;
}

void Device::createLogicalDevice() {
    CTH_CRITICAL(_queueFamiliesQueueCounts.empty(), "queue family queue count must be queried first") {}
    CTH_CRITICAL(created(), "device was already created") {}

    vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    queueCreateInfos.reserve(_queueFamiliesQueueCounts.size());

    constexpr float queuePriority = 1.0f;
    for(auto const [queueFamily, queueCount] : _queueFamiliesQueueCounts)
        queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily,
            .queueCount = queueCount,
            .pQueuePriorities = &queuePriority,
        });

    auto const requiredExtensions = str::to_c_str_vector(_physicalDevice->requiredExtensions());


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
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create logical device"){
        reset();
        throw cth::vk::result_exception{createResult, details->exception()};
    }
    _handle = ptr;
}
void Device::createFunctionTable() {
    CTH_CRITICAL(_functionTable != nullptr, "function table must be nullptr") {}

    _functionTable = std::make_unique<VolkDeviceTable>();
    volkLoadDeviceTable(_functionTable.get(), get());

}

void Device::wrapQueues(span<uint32_t const> family_indices, span<Queue> queues) const {
    CTH_CRITICAL(family_indices.size() != queues.size(), "there must be a family index for every queue") {}

    std::unordered_map<uint32_t, uint32_t> queueCounts{};
    for(auto& index : family_indices) queueCounts[index] = 0;

    for(auto [familyIndex, queue] : std::views::zip(family_indices, queues)) {
        VkQueue ptr = VK_NULL_HANDLE;
        table()->vkGetDeviceQueue(_handle.get(), familyIndex, queueCounts[familyIndex], &ptr);

        CTH_STABLE_ERR(ptr == VK_NULL_HANDLE, "failed to get device queue") throw details->exception();

        queue.wrap(Queue::State{ptr, familyIndex, queueCounts[familyIndex]++});
    }

}
Device::State Device::release() {
    DEBUG_CHECK_DEVICE(this);

    State state{_handle.release(), std::move(_queueFamiliesQueueCounts)};
    reset();
    return state;
}
void Device::waitIdle() const {
    DEBUG_CHECK_DEVICE(this);

    auto const result = table()->vkDeviceWaitIdle(_handle.get());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to wait for device") throw vk::result_exception{result, details->exception()};
}
void Device::destroy(VkDevice vk_device) {
    vkDestroyDevice(vk_device, nullptr);

    cth::log::msg<except::LOG>("destroyed vk_device");
}
void Device::reset() {
    _handle = nullptr;
    _queueFamiliesQueueCounts.clear();
    _functionTable = {};
}


#ifdef CONSTANT_DEBUG_MODE
void Device::debug_check(cth::not_null<Device const*> device) {
    CTH_ERR(!device->created(), "device must be created") throw details->exception();
    DEBUG_CHECK_DEVICE_HANDLE(device->get());
}
void Device::debug_check_handle([[maybe_unused]] vk::not_null<VkDevice> vk_device) {}
#endif

}
