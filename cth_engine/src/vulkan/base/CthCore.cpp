#include "CthCore.hpp"

#include "CthDevice.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"

namespace cth::vk {
void BasicCore::wrap(move_ptr<BasicInstance> const instance, move_ptr<PhysicalDevice> const physical_device,
    move_ptr<Device> const device) {
    DEBUG_CHECK_INSTANCE(instance.get());
    DEBUG_CHECK_PHYSICAL_DEVICE(physical_device.get());
    DEBUG_CHECK_DEVICE(device.get());

    _device = device;
    _physicalDevice = physical_device;
    _instance = instance;
}
void BasicCore::create(Config const& config) {
    DEBUG_CHECK_CORE_LEAK(this);
    auto instance = std::make_unique<Instance>(config.appName, config.requiredExtensions);
    auto physicalDevice = PhysicalDevice::AutoPick(instance.get(), config.queues, {}, {});
    auto device = std::make_unique<Device>(instance.get(), physicalDevice.get(), config.queues);

    BasicCore::wrap(instance.release(), physicalDevice.release(), device.release());
}
void BasicCore::destroy() {
    delete _device.get(); //TEMP add basic device and destroy()
    _device = nullptr;
    delete _physicalDevice.get(); //TEMP add basic device and destroy()
    _physicalDevice = nullptr;

    _instance->destroy();
}
void BasicCore::reset() {
    DEBUG_CHECK_CORE(this);
    _device = nullptr;
    _physicalDevice = nullptr;
    _instance = nullptr;
}


Device const* BasicCore::device() const { return _device.get(); }
VkDevice BasicCore::vkDevice() const { return _device->get(); }
PhysicalDevice const* BasicCore::physicalDevice() const { return _physicalDevice.get(); }
VkPhysicalDevice BasicCore::vkPhysicalDevice() const { return _physicalDevice->get(); }
BasicInstance const* BasicCore::instance() const { return _instance.get(); }
VkInstance BasicCore::vkInstance() const { return _instance->get(); }

#ifdef CONSTANT_DEBUG_MODE
void BasicCore::debug_check(BasicCore const* core) {
    CTH_ERR(core == nullptr, "core invalid (nullptr)") throw details->exception();
    DEBUG_CHECK_DEVICE(core->device());
    DEBUG_CHECK_PHYSICAL_DEVICE(core->physicalDevice());
    DEBUG_CHECK_INSTANCE(core->instance());
}
void BasicCore::debug_check_leak(BasicCore const* core) {
    if(core) {
        CTH_WARN(core->device(), "device replaced (potential memory leak)") {}
        CTH_WARN(core->physicalDevice(), "physical device replaced (potential memory leak)") {}
        CTH_WARN(core->instance(), "instance replaced (potential memory leak)") {}
    }
}
#endif
}


namespace cth::vk {
Core::Core(Config const& config) { BasicCore::create(config); }
Core::~Core() {
    Core::destroy();
} 

void Core::wrap(move_ptr<BasicInstance> const instance, move_ptr<PhysicalDevice> const physical_device, move_ptr<Device> const device) {
    if(BasicCore::device() || BasicCore::physicalDevice() || BasicCore::instance()) Core::destroy();
    BasicCore::wrap(instance, physical_device, device);
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
void Core::release() {
    BasicCore::reset();
}



}
