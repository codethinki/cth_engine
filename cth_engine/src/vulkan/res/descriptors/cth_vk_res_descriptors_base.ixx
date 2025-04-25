module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>

export module cth.vk.res.descriptors.base;

import cth.vk.base.device;
import cth.vk.render.exec.pipeline;

import cth.io.log;

export namespace cth::vk {
//TEMP rename this to DescriptorBase

class Descriptor {
public:
    explicit Descriptor(VkDescriptorType type) : _vkType(type) {}
    virtual ~Descriptor() = 0;


    [[nodiscard]] virtual VkDescriptorBufferInfo bufferInfo() const {
        CTH_ERR(true, "invalid function call, no buffer info present") throw details->exception(); //TEMP change this
    }
    [[nodiscard]] virtual VkDescriptorImageInfo imageInfo() const {
        CTH_ERR(true, "invalid function call, no image info present") throw details->exception(); //TEMP change this
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
