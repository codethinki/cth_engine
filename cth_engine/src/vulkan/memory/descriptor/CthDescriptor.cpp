#include "CthDescriptor.hpp"

#include <vulkan/utility/CthVkUtils.hpp>
#include "CthDescriptedResource.hpp"


//Descriptor

namespace cth {
Descriptor::Descriptor(const VkDescriptorType type, const DescriptedResource& resource, const VkDeviceSize size,
    const VkDeviceSize resource_offset) : vkType(type), resInfo(resource.descriptorInfo(size, resource_offset)) {}
}
