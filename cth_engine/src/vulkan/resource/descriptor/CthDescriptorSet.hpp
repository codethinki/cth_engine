#pragma once
#include <span>
#include <vulkan/vulkan.h>

#include <vector>

#include "../buffer/CthBasicBuffer.hpp"

namespace cth {
using namespace std;


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
    explicit DescriptorSet(const Builder& builder);
    virtual ~DescriptorSet();

private:
    void alloc(VkDescriptorSet set, DescriptorPool* pool);
    void deallocate();
    [[nodiscard]] virtual vector<VkWriteDescriptorSet> writes();

    void clearDescriptors() { _descriptors.clear(); }

    void copyInfos();

    [[nodiscard]] static InfoType infoType(VkDescriptorType descriptor_type);


    const DescriptorSetLayout* _layout;
    vector<vector<Descriptor*>> _descriptors{};
    vector<VkDescriptorBufferInfo> _bufferInfos{};
    vector<VkDescriptorImageInfo> _imageInfos{};

    mem::basic_ptr<VkDescriptorSet_T> _vkSet = VK_NULL_HANDLE;
    bool _written = false;

    DescriptorPool* _pool = nullptr;

    friend DescriptorPool;

public:
    [[nodiscard]] VkDescriptorSet get() const { return _vkSet.get(); }
    [[nodiscard]] bool written() const { return _written; }

    struct Builder {
        explicit Builder(const DescriptorSetLayout* layout);

        /**
         *\note in this constructor only one resource per binding can be specified
         */
        explicit Builder(const DescriptorSetLayout* layout, span<Descriptor* const> descriptors, uint32_t binding_offset = 0);

        Builder& addDescriptor(Descriptor* descriptor, uint32_t binding, uint32_t arr_index = 0);
        Builder& addDescriptors(span<Descriptor* const> binding_descriptors, uint32_t binding, uint32_t arr_first);

        Builder& removeDescriptor(uint32_t binding, uint32_t arr_index);
        Builder& removeDescriptors(uint32_t binding, uint32_t arr_first, uint32_t count);

    private:
        void init(const DescriptorSetLayout* layout);

        const DescriptorSetLayout* _layout;
        vector<vector<Descriptor*>> _descriptors{};

        friend DescriptorSet;
    };

    DescriptorSet(const DescriptorSet& other) = delete;
    DescriptorSet(DescriptorSet&& other) = delete;
    DescriptorSet& operator=(const DescriptorSet& other) = delete;
    DescriptorSet& operator=(DescriptorSet&& other) = delete;
};

}
