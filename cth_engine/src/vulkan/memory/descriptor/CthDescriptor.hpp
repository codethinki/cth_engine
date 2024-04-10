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
    explicit Descriptor(const VkDescriptorType type) : vkType(type) {}
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

public:
    [[nodiscard]] VkDescriptorType type() const { return vkType; }

    Descriptor(const Descriptor& other) = default;
    Descriptor(Descriptor&& other) = delete;
    Descriptor& operator=(const Descriptor& other) = default;
    Descriptor& operator=(Descriptor&& other) = delete;
};

inline Descriptor::~Descriptor() = default;

}
