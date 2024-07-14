#include "CthCore.hpp"

#include "CthDevice.hpp"
#include "CthInstance.hpp"
#include "CthPhysicalDevice.hpp"

namespace cth {
void BasicCore::wrap(const move_ptr<BasicInstance> instance, const move_ptr<PhysicalDevice> physical_device,
    const move_ptr<Device> device) {
    DEBUG_CHECK_INSTANCE(instance.get());
    DEBUG_CHECK_PHYSICAL_DEVICE(physical_device.get());
    DEBUG_CHECK_DEVICE(device.get());

    _device = device;
    _physicalDevice = physical_device;
    _instance = instance;
}
void BasicCore::create(const Config& config) {
    DEBUG_CHECK_CORE_LEAK(this);
    auto instance = std::make_unique<Instance>(config.appName, config.requiredExtensions);
    auto physicalDevice = PhysicalDevice::autoPick(*instance, config.queues);
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


const Device* BasicCore::device() const { return _device.get(); }
VkDevice BasicCore::vkDevice() const { return _device->get(); }
const PhysicalDevice* BasicCore::physicalDevice() const { return _physicalDevice.get(); }
VkPhysicalDevice BasicCore::vkPhysicalDevice() const { return _physicalDevice->get(); }
const BasicInstance* BasicCore::instance() const { return _instance.get(); }
VkInstance BasicCore::vkInstance() const { return _instance->get(); }

#ifdef CONSTANT_DEBUG_MODE
void BasicCore::debug_check(const BasicCore* core) {
    CTH_ERR(core == nullptr, "core invalid (nullptr)") throw details->exception();
    DEBUG_CHECK_DEVICE(core->device());
    DEBUG_CHECK_PHYSICAL_DEVICE(core->physicalDevice());
    DEBUG_CHECK_INSTANCE(core->instance());
}
void BasicCore::debug_check_leak(const BasicCore* core) {
    if(core) {
        CTH_WARN(core->device(), "device replaced (potential memory leak)") {}
        CTH_WARN(core->physicalDevice(), "physical device replaced (potential memory leak)") {}
        CTH_WARN(core->instance(), "instance replaced (potential memory leak)") {}
    }
}
#endif
}


namespace cth {
Core::Core(const Config& config) { BasicCore::create(config); }
Core::~Core() {
    Core::destroy();
} 

void Core::wrap(const move_ptr<BasicInstance> instance, const move_ptr<PhysicalDevice> physical_device, const move_ptr<Device> device) {
    if(BasicCore::device() || BasicCore::physicalDevice() || BasicCore::instance()) Core::destroy();
    BasicCore::wrap(instance, physical_device, device);
}
void Core::create(const Config& config) {
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
