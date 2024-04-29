#pragma once
#include <span>
#include <vulkan/vulkan.h>

#include <vector>

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

    void clearDescriptors() { descriptors.clear(); }

    void copyInfos();

    [[nodiscard]] static InfoType infoType(VkDescriptorType type);


    DescriptorSetLayout* layout;
    vector<vector<Descriptor*>> descriptors{};
    vector<VkDescriptorBufferInfo> bufferInfos{};
    vector<VkDescriptorImageInfo> imageInfos{};

    VkDescriptorSet vkSet = VK_NULL_HANDLE;
    bool written_ = false;

    DescriptorPool* pool = nullptr;
    friend DescriptorPool;

public:
    [[nodiscard]] VkDescriptorSet get() const { return vkSet; }
    [[nodiscard]] bool written() const { return written_; }

    struct Builder {
        explicit Builder(DescriptorSetLayout* layout);

        /**
         *\note in this constructor only one resource per binding can be specified
         */
        explicit Builder(DescriptorSetLayout* layout, span<Descriptor* const> descriptors, uint32_t binding_offset = 0);

        Builder& addDescriptor(Descriptor* descriptor, uint32_t binding, uint32_t arr_index = 0);
        Builder& addDescriptors(span<Descriptor* const> binding_descriptors, uint32_t binding, uint32_t arr_first);

        Builder& removeDescriptor(uint32_t binding, uint32_t arr_index);
        Builder& removeDescriptors(uint32_t binding, uint32_t arr_first, uint32_t count);

    private:
        void init(const DescriptorSetLayout* set_layout);

        DescriptorSetLayout* layout;
        vector<vector<Descriptor*>> descriptors{};

        friend DescriptorSet;
    };

    DescriptorSet(const DescriptorSet& other) = delete;
    DescriptorSet(DescriptorSet&& other) = delete;
    DescriptorSet& operator=(const DescriptorSet& other) = delete;
    DescriptorSet& operator=(DescriptorSet&& other) = delete;
};

}
