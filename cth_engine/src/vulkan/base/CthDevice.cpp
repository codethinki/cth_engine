#include "CthDevice.hpp"


#include "CthCore.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"
#include "CthQueue.hpp"
#include "vulkan/utility/cth_vk_utils.hpp"


namespace cth::vk {
using std::vector;
using std::string_view;
using std::span;

Device::Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device) :
    _instance{instance}, _physicalDevice{physical_device} {}
Device::Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device, State const& state) : Device{instance,
    physical_device} { wrap(state); }
Device::Device(cth::not_null<Instance const*> instance, cth::not_null<PhysicalDevice const*> physical_device, std::span<Queue> queues) : Device{
    instance, physical_device} { create(queues); }
Device::~Device() { optDestroy(); }
void Device::wrap(State const& state) {
    _handle = state.handle.get();
    _uniqueFamilyIndices = state.familyIndices;
}
void Device::create(std::span<Queue> queues) {
    createLogicalDevice();
    auto const familyIndices = setUniqueFamilyIndices(queues);
    wrapQueues(familyIndices, queues);
}
void Device::destroy() {
    DEBUG_CHECK_DEVICE(this);

    destroy(_handle.release());
    reset();
}

vector<uint32_t> Device::setUniqueFamilyIndices(span<Queue const> queues) {
    auto const& queueFamilyIndices = _physicalDevice->queueFamilyIndices(queues);

    _uniqueFamilyIndices = queueFamilyIndices | std::ranges::to<std::unordered_set<uint32_t>>() | std::ranges::to<vector<uint32_t>>();

    return queueFamilyIndices;
}

void Device::createLogicalDevice() {
    vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;
    std::ranges::for_each(_uniqueFamilyIndices, [&queueCreateInfos, queuePriority](uint32_t queue_family) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queue_family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    });

    auto const requiredExtensions = utils::to_c_str_vector(_physicalDevice->requiredExtensions());

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = _physicalDevice->requiredFeatures().get();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();


    createInfo.enabledExtensionCount = static_cast<uint32_t>(constants::REQUIRED_DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    VkDevice ptr = VK_NULL_HANDLE;

    VkResult const createResult = vkCreateDevice(_physicalDevice->get(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "failed to create logical device")
        throw cth::vk::result_exception{createResult, details->exception()};

    _handle = ptr;
}
void Device::wrapQueues(span<uint32_t const> family_indices, span<Queue> queues) const {
    auto queueCounts = family_indices | std::views::transform([](uint32_t index) { return std::pair{index, 0}; }) | std::ranges::to<
        std::unordered_map<uint32_t, uint32_t>>();

    for(auto [index, queue] : std::views::zip(family_indices, queues)) {
        VkQueue ptr = VK_NULL_HANDLE;
        vkGetDeviceQueue(_handle.get(), index, queueCounts[index], &ptr);

        queue.wrap(index, queueCounts[index], ptr);
    }

}
Device::State Device::release() {
    State state{_handle.release(), std::move(_uniqueFamilyIndices)};
    reset();
    return state;
}
void Device::waitIdle() const {
    auto const result = vkDeviceWaitIdle(_handle.get());

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to wait for device") throw vk::result_exception{result, details->exception()};
}
void Device::destroy(VkDevice vk_device) {
    vkDestroyDevice(vk_device, nullptr);

    cth::log::msg<except::LOG>("destroyed vk_device");
}
void Device::reset() {
    _handle = nullptr;
    _uniqueFamilyIndices.clear();
}


#ifdef CONSTANT_DEBUG_MODE
void Device::debug_check(cth::not_null<Device const*> device) {
    CTH_ERR(!device->created(), "device must be created") throw details->exception();
    DEBUG_CHECK_DEVICE_HANDLE(device->get());
}
void Device::debug_check_handle([[maybe_unused]] vk::not_null<VkDevice> vk_device) {}
#endif

}
