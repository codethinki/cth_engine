#include "CthCore.hpp"

#include "CthDevice.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"

namespace cth::vk {

Core::Core(State state) { wrap(std::move(state)); }
Core::Core(Config const& config) { create(config); }
Core::~Core() { optDestroy(); }

void Core::wrap(State state) {
    DEBUG_CHECK_INSTANCE(state.instance.get());
    DEBUG_CHECK_PHYSICAL_DEVICE(state.physicalDevice.get());
    DEBUG_CHECK_DEVICE(state.device.get());
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
    _physicalDevice = PhysicalDevice::AutoPick(_instance.get(), config.queues, {}, {});
    _device = std::make_unique<Device>(_instance.get(), _physicalDevice.get(), config.queues);

    if(config.destructionQueue) _destructionQueue = std::make_unique<DestructionQueue>();
}
void Core::destroy() {
    debug_check(this);

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




Device const* Core::device() const { return _device.get(); }
VkDevice Core::vkDevice() const { return _device->get(); }
PhysicalDevice const* Core::physicalDevice() const { return _physicalDevice.get(); }
VkPhysicalDevice Core::vkPhysicalDevice() const { return _physicalDevice->get(); }
Instance const* Core::instance() const { return _instance.get(); }
VkInstance Core::vkInstance() const { return _instance->get(); }
DestructionQueue* Core::destructionQueue() const { return _destructionQueue.get(); }

}
