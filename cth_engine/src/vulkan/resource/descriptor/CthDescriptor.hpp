#pragma once

#include<cth/io/log.hpp>

#include <vulkan/vulkan.h>

namespace cth::vk {

class Device;
class Pipeline;
class DescriptedResource;



class Descriptor {
public:
    explicit Descriptor(VkDescriptorType  type) : _vkType(type) {}
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
    VkDescriptorType _vkType;

public:
    [[nodiscard]] VkDescriptorType type() const { return _vkType; }

    Descriptor(Descriptor const& other) = default;
    Descriptor(Descriptor&& other) = delete;
    Descriptor& operator=(Descriptor const& other) = default;
    Descriptor& operator=(Descriptor&& other) = delete;
};

inline Descriptor::~Descriptor() = default;

}
