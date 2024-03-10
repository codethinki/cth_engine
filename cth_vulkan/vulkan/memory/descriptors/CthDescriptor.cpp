#include "CthDescriptor.hpp"

#include "CthDescriptedResource.hpp"
#include "../../core/CthDevice.hpp"
#include "../../utils/cth_vk_specific_utils.hpp"


//Descriptor

namespace cth {
Descriptor::Descriptor(const VkDescriptorType type, const DescriptedResource& resource, const VkDeviceSize size,
    const VkDeviceSize resource_offset) : vkType(type), resInfo(resource.descriptorInfo(size, resource_offset)) {}
}
