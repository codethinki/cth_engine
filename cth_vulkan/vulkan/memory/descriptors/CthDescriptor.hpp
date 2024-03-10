#pragma once
#include <cth/cth_log.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>


namespace cth {
using namespace std;

class Device;
class Pipeline;
class DescriptedResource;



class Descriptor {
public:
    using descriptor_info_t = optional<variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>>;

    [[nodiscard]] VkDescriptorBufferInfo bufferInfo() const {
        CTH_ERR(resInfo == std::nullopt || !holds_alternative<VkDescriptorBufferInfo>(*resInfo), "invalid, no buffer info present")
            throw details->exception();
        return get<VkDescriptorBufferInfo>(*resInfo);
    }
    [[nodiscard]] VkDescriptorImageInfo imageInfo() const {
        CTH_ERR(resInfo == std::nullopt || !holds_alternative<VkDescriptorImageInfo>(*resInfo), "invalid, no image info present")
            throw details->exception();
        return get<VkDescriptorImageInfo>(*resInfo);
    }


    Descriptor(VkDescriptorType type, const DescriptedResource& resource, VkDeviceSize size, VkDeviceSize resource_offset);
    Descriptor(const VkDescriptorType type, const descriptor_info_t& info) : vkType(type), resInfo(info) {}

private:
    VkDescriptorType vkType;
    descriptor_info_t resInfo;

public:
    [[nodiscard]] VkDescriptorType type() const { return vkType; }

    Descriptor(const Descriptor& other) = delete;
    Descriptor(Descriptor&& other) = delete;
    Descriptor& operator=(const Descriptor& other) = delete;
    Descriptor& operator=(Descriptor&& other) = delete;
};

}






