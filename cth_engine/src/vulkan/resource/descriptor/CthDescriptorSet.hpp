#pragma once
#include "../buffer/CthBasicBuffer.hpp"


#include <vulkan/vulkan.h>

#include <span>
#include <vector>


namespace cth::vk {


class Descriptor;
class DescriptorSetLayout;
class DescriptorPool;


class DescriptorSet {
    enum class InfoType {
        BUFFER,
        IMAGE,
        NONE
    };

public:
    struct Builder;
    explicit DescriptorSet(Builder const& builder);
    virtual ~DescriptorSet();

private:
    void alloc(VkDescriptorSet set, DescriptorPool* pool);
    void deallocate();
    [[nodiscard]] virtual std::vector<VkWriteDescriptorSet> writes();

    void clearDescriptors() { _descriptors.clear(); }

    void copyInfos();

    [[nodiscard]] static InfoType infoType(VkDescriptorType descriptor_type);


    DescriptorSetLayout const* _layout;
    std::vector<std::vector<Descriptor*>> _descriptors{};
    std::vector<VkDescriptorBufferInfo> _bufferInfos{};
    std::vector<VkDescriptorImageInfo> _imageInfos{};

    move_ptr<VkDescriptorSet_T> _vkSet = VK_NULL_HANDLE;
    bool _written = false;

    DescriptorPool* _pool = nullptr;

    friend DescriptorPool;

public:
    [[nodiscard]] VkDescriptorSet get() const { return _vkSet.get(); }
    [[nodiscard]] bool written() const { return _written; }

    struct Builder {
        explicit Builder(DescriptorSetLayout const* layout);

        /**
         *@note in this constructor only one resource per binding can be specified
         */
        explicit Builder(DescriptorSetLayout const* layout, std::span<Descriptor* const> descriptors, uint32_t binding_offset = 0);

        Builder& addDescriptor(Descriptor* descriptor, uint32_t binding, uint32_t arr_index = 0);
        Builder& addDescriptors(std::span<Descriptor* const> binding_descriptors, uint32_t binding, uint32_t arr_first);

        Builder& removeDescriptor(uint32_t binding, uint32_t arr_index);
        Builder& removeDescriptors(uint32_t binding, uint32_t arr_first, uint32_t count);

    private:
        void init(DescriptorSetLayout const* layout);

        DescriptorSetLayout const* _layout;
        std::vector<std::vector<Descriptor*>> _descriptors{};

        friend DescriptorSet;
    };

    DescriptorSet(DescriptorSet const& other) = delete;
    DescriptorSet(DescriptorSet&& other) = delete;
    DescriptorSet& operator=(DescriptorSet const& other) = delete;
    DescriptorSet& operator=(DescriptorSet&& other) = delete;
};

}
