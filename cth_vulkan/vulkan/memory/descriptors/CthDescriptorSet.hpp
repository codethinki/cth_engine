#pragma once
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
    virtual ~DescriptorSet();

protected:
    [[nodiscard]] virtual vector<VkWriteDescriptorSet> writes();

    void clearDescriptors() { descriptors.clear(); }

private:
    void copyInfos();

    [[nodiscard]] static InfoType infoType(VkDescriptorType type);


    DescriptorSetLayout* layout;
    vector<vector<Descriptor*>> descriptors{};
    vector<VkDescriptorBufferInfo> bufferInfos{};
    vector<VkDescriptorImageInfo> imageInfos{};

    VkDescriptorSet vkSet = VK_NULL_HANDLE;
    bool allocState = false;

    DescriptorPool* pool = nullptr;
    friend DescriptorPool;

public:
    DescriptorSet(const DescriptorSet& other) = delete;
    DescriptorSet(DescriptorSet&& other) = delete;
    DescriptorSet& operator=(const DescriptorSet& other) = delete;
    DescriptorSet& operator=(DescriptorSet&& other) = delete;


    [[nodiscard]] VkDescriptorSet get() const { return vkSet; }
    [[nodiscard]] bool allocated() const { return allocState; }
};


/**
 * \brief caches the set allocation writes, useful for sets persisting though pool.reset()
 */
class GlobalDescriptorSet : public DescriptorSet {
public:
    explicit GlobalDescriptorSet(const Builder& builder);
    ~GlobalDescriptorSet() override = default;

protected:
    [[nodiscard]] vector<VkWriteDescriptorSet> writes() override;

private:
    vector<VkWriteDescriptorSet> _writes;

public:
    GlobalDescriptorSet(const GlobalDescriptorSet& other) = delete;
    GlobalDescriptorSet(GlobalDescriptorSet&& other) = delete;
    GlobalDescriptorSet& operator=(const GlobalDescriptorSet& other) = delete;
    GlobalDescriptorSet& operator=(GlobalDescriptorSet&& other) = delete;
};

}
