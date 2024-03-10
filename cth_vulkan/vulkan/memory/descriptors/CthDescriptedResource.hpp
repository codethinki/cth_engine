#pragma once
#include "CthDescriptor.hpp"


//TEMP left off here, create a virtual base class for any vk object that might return a descriptor info
//use std::variant in the descriptorInfo thing to store different types of descriptor info
//also change the Image and Buffer classes accordingly and make them publicly inherit from this virtual base class

namespace cth {
using std::variant;
using std::optional;

class DescriptedResource {

public:
    explicit DescriptedResource() = default;
    virtual ~DescriptedResource() = default;
    /**
    * \param size in bytes, VK_WHOLE_SIZE -> whole buffer
    * \param offset in bytes
    */
    [[nodiscard]] virtual Descriptor::descriptor_info_t descriptorInfo(VkDeviceSize size, VkDeviceSize offset) const = 0;
    [[nodiscard]] Descriptor::descriptor_info_t descriptorInfo() const { return descriptorInfo(VK_WHOLE_SIZE, 0); }

    DescriptedResource(const DescriptedResource& other) = delete;
    DescriptedResource(DescriptedResource&& other) = default;
    DescriptedResource& operator=(const DescriptedResource& other) = delete;
    DescriptedResource& operator=(DescriptedResource&& other) = default;
};
}
