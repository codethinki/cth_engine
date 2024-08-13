#include "CthCore.hpp"

#include "CthDevice.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"

#include "vulkan/resource/CthDestructionQueue.hpp"

namespace cth::vk {
void BasicCore::wrap(BasicInstance* instance, PhysicalDevice* physical_device, Device* device, DestructionQueue* destruction_queue) {
    DEBUG_CHECK_INSTANCE(instance);
    DEBUG_CHECK_PHYSICAL_DEVICE(physical_device);
    DEBUG_CHECK_DEVICE(device);
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(destruction_queue);

    _destructionQueue = destruction_queue;
    _device = device;
    _physicalDevice = physical_device;
    _instance = instance;
}
void BasicCore::create(Config const& config) {
    DEBUG_CHECK_CORE_LEAK(this);
    auto const instance = new Instance(config.appName, config.requiredExtensions);
    auto const physicalDevice = PhysicalDevice::AutoPick(instance, config.queues, {}, {}).release();
    auto const device = new Device(instance, physicalDevice, config.queues);
    auto const destructionQueue = new DestructionQueue(device, physicalDevice, instance);

    BasicCore::wrap(instance, physicalDevice, device, destructionQueue);
}
void BasicCore::destroy() {
    if(_destructionQueue) {
        delete _destructionQueue.get();
        _destructionQueue = nullptr;
    }

    delete _device.get(); //TEMP add basic device and destroy()
    _device = nullptr;
    delete _physicalDevice.get(); //TEMP add basic device and destroy()
    _physicalDevice = nullptr;
    _instance->destroy();
    delete _instance.get();

    _instance = nullptr;
}
void BasicCore::reset() {
    DEBUG_CHECK_CORE(this);

    _destructionQueue = nullptr;
    _device = nullptr;
    _physicalDevice = nullptr;
    _instance = nullptr;
}
BasicCore::Ptrs BasicCore::release() {
    Ptrs const temp{
        _instance.get(),
        _physicalDevice.get(),
        _device.get(),
        _destructionQueue.get(),
    };

    BasicCore::reset();

    return temp;
}


Device const* BasicCore::device() const { return _device.get(); }
VkDevice BasicCore::vkDevice() const { return _device->get(); }
PhysicalDevice const* BasicCore::physicalDevice() const { return _physicalDevice.get(); }
VkPhysicalDevice BasicCore::vkPhysicalDevice() const { return _physicalDevice->get(); }
BasicInstance const* BasicCore::instance() const { return _instance.get(); }
VkInstance BasicCore::vkInstance() const { return _instance->get(); }
DestructionQueue* BasicCore::destructionQueue() const { return _destructionQueue.get(); }

#ifdef CONSTANT_DEBUG_MODE
void BasicCore::debug_check(BasicCore const* core) {
    CTH_ERR(core == nullptr, "core invalid (nullptr)") throw details->exception();
    DEBUG_CHECK_DESTRUCTION_QUEUE_NULL_ALLOWED(core->_destructionQueue.get());
    DEBUG_CHECK_DEVICE(core->_device.get());
    DEBUG_CHECK_PHYSICAL_DEVICE(core->_physicalDevice.get());
    DEBUG_CHECK_INSTANCE(core->_instance.get());
}
void BasicCore::debug_check_leak(BasicCore const* core) {
    if(core == nullptr) return;

    CTH_WARN(core->device(), "device replaced (potential memory leak)") {}
    CTH_WARN(core->physicalDevice(), "physical device replaced (potential memory leak)") {}
    CTH_WARN(core->instance(), "instance replaced (potential memory leak)") {}
    CTH_WARN(core->_destructionQueue, "destruction-queue replaced (potential memory leak)") {}
}
#endif
}


namespace cth::vk {
Core::Core(Config const& config) { BasicCore::create(config); }
Core::~Core() { Core::destroy(); }

void Core::wrap(BasicInstance* instance, PhysicalDevice* physical_device, Device* device, DestructionQueue* destruction_queue) {
    if(BasicCore::device() || BasicCore::physicalDevice() || BasicCore::instance()) Core::destroy();
    BasicCore::wrap(instance, physical_device, device, destruction_queue);
}
void Core::create(Config const& config) {
    if(BasicCore::device() || BasicCore::physicalDevice() || BasicCore::instance()) Core::destroy();
    BasicCore::create(config);
}

void Core::reset() {
    delete BasicCore::device();
    delete BasicCore::physicalDevice();
    delete BasicCore::instance();
    BasicCore::reset();
}

}
