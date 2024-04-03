#pragma once

#include <cth/cth_log.hpp>

#include <vulkan/vulkan.h>

namespace cth {
using namespace std;

class Device;
class Pipeline;
class DescriptedResource;



class Descriptor {
public:
    Descriptor(const VkDescriptorType type, const VkDeviceSize size, const VkDeviceSize resource_offset) : vkType(type),
        _size(size), _offset(resource_offset) {}
    virtual ~Descriptor() = 0;


    [[nodiscard]] virtual VkDescriptorBufferInfo bufferInfo() const {
        CTH_ERR(true, "invalid function call, no buffer info present")
            throw details->exception();
    }
    [[nodiscard]] virtual VkDescriptorImageInfo imageInfo() const {
        CTH_ERR(true, "invalid function call, no image info present")
            throw details->exception();
    }

private:
    VkDescriptorType vkType;
    size_t _size, _offset;

public:
    [[nodiscard]] VkDescriptorType type() const { return vkType; }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] size_t offset() const { return _offset; }

    Descriptor(const Descriptor& other) = default;
    Descriptor(Descriptor&& other) = delete;
    Descriptor& operator=(const Descriptor& other) = default;
    Descriptor& operator=(Descriptor&& other) = delete;
};


}
