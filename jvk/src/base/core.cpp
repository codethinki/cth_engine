#include "jvk/base/core.hpp"

#include "jvk/base/device.hpp"
#include "jvk/base/instance.hpp"
#include "jvk/base/physical_device.hpp"
#include "jvk/res/destruction_queue.hpp"
#include "jvk/utility/os.hpp"

namespace jvk {

Core::Core(State state) { wrap(std::move(state)); }
Core::Core(Config const& config) { create(config); }

Core::~Core() {
    optDestroy();
}

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
    _physicalDevice = PhysicalDevice::AutoPick(*_instance, *jvk::os::TempSurface(*_instance), config.queues,
        {}, {});

    _device = std::make_unique<Device>(*_instance, *_physicalDevice, config.queues);

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
