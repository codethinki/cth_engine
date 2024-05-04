#pragma once
#include "vulkan/base/CthDevice.hpp"


namespace cth {
class BasicInstance;
class PhysicalDevice;
class Device;

class Context {
public:
    Context(Device* device, PhysicalDevice* physical_device, BasicInstance* instance) : _device(device), _physicalDevice(physical_device),
        _instance(instance) {}

private:
    Device* _device;
    PhysicalDevice* _physicalDevice;
    BasicInstance* _instance;

public:
    [[nodiscard]] Device* device() const { return _device; }
    [[nodiscard]] PhysicalDevice* physicalDevice() const { return _physicalDevice; }
    [[nodiscard]] BasicInstance* instance() const { return _instance; }
};

}
