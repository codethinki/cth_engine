#pragma once
#include <cth/cth_log.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <variant>
#include <vector>


namespace cth {
using namespace std;

class Device;
class Pipeline;
class DescriptedResource;


class DescriptorPool;

class Descriptor {
public:
    using descriptor_info_t = optional<variant<VkDescriptorBufferInfo, VkDescriptorImageInfo>>;

    [[nodiscard]] VkDescriptorBufferInfo bufferInfo() const {
        CTH_ERR(resInfo != std::nullopt && holds_alternative<VkDescriptorBufferInfo>(*resInfo), "invalid, no buffer info present")
            throw details->exception();
        return get<VkDescriptorBufferInfo>(*resInfo);
    }
    [[nodiscard]] VkDescriptorImageInfo imageInfo() const {
        CTH_ERR(resInfo != std::nullopt && holds_alternative<VkDescriptorImageInfo>(*resInfo), "invalid, no image info present")
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
    Descriptor(Descriptor&& other) = default;
    Descriptor& operator=(const Descriptor& other) = delete;
    Descriptor& operator=(Descriptor&& other) = default;
};
//TODO create a ShaderStageCollection class for managing shaders and the descriptor layout
class DescriptorSetLayout {
public:
    struct Builder {
        Builder() = default;
        Builder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags flags, uint32_t count = 1);
        Builder& removeBinding(uint32_t binding);

    private:
        vector<VkDescriptorSetLayoutBinding> bindings{};
        friend DescriptorSetLayout;
    };

    /**
     * \brief creates a DescriptorSetLayout with the copied builder data
     */
    explicit DescriptorSetLayout(const Device& device, const Builder& builder);

private:
    VkDescriptorSetLayout vkLayout = VK_NULL_HANDLE;
    vector<VkDescriptorSetLayoutBinding> vkBindings{};

public:
    [[nodiscard]] VkDescriptorSetLayout get() const { return vkLayout; }
    [[nodiscard]] vector<VkDescriptorSetLayoutBinding> bindings() const { return vkBindings; }
    [[nodiscard]] VkDescriptorSetLayoutBinding binding(const uint32_t binding) const { return vkBindings[binding]; }
    [[nodiscard]] VkDescriptorType bindingType(const uint32_t binding) const { return vkBindings[binding].descriptorType; }
};

class DescriptorSet {
public:
    struct Builder {
        explicit Builder(DescriptorSetLayout* layout);
        Builder& addDescriptor(Descriptor* descriptor, uint32_t binding, uint32_t arr_index = 0);
        Builder& addDescriptors(const vector<Descriptor*>& descriptors, uint32_t binding, uint32_t arr_first);

        Builder& removeDescriptor(uint32_t binding, uint32_t arr_index);
        Builder& removeDescriptors(uint32_t binding, uint32_t arr_first, uint32_t count);
        
    private:
        DescriptorSetLayout* layout;
        vector<vector<Descriptor*>> descriptors{};

        friend DescriptorSet;
    };

    explicit DescriptorSet(const Builder& builder);

    void alloc(DescriptorPool* pool);

private:
    VkDescriptorSet vkSet = VK_NULL_HANDLE;
    vector<VkWriteDescriptorSet> writes{};

    bool allocated = false;

    vector<VkDescriptorBufferInfo> bufferInfos{};
    vector<VkDescriptorImageInfo> imageInfos{};

public:
    [[nodiscard]] VkDescriptorSet get() const { return vkSet; }
};

class DescriptorPool {
public:
    /**
     * \param max_descriptor_uses [type, count] pairs -> limit for allocated descriptor uses per type
     */
    DescriptorPool(Device* device, uint32_t max_allocated_sets, unordered_map<VkDescriptorType, uint32_t> max_descriptor_uses);
    ~DescriptorPool();

    uint32_t addPipeline(Pipeline* pipeline);
    uint32_t getPipelineId(Pipeline* pipeline);

    DescriptorSet* newSet(DescriptorSetLayout* layout);

    Descriptor* newDescriptor(DefaultBuffer* buffer);
    Descriptor* newDescriptor(Image* image);

    void bind(uint32_t pipeline_id);
    void bind(Pipeline* pipeline);

    void alloc(uint32_t set_id);
    void create();
    void destroy();

private:
    bool allocated = false;
};
}
