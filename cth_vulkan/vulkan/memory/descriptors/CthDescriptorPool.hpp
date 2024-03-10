#pragma once
#include <vulkan/vulkan.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cth {
class Device;
class DescriptorSet;
class DescriptorSetLayout;

using namespace std;

/**
 * \brief wrapper class for the VkDescriptorPool
 * \note the pool allocates the max_descriptor_sets instantly
 * \note all DescriptorSetLayout's ever used with the pool must be known at its creation
 */
class DescriptorPool {
public:
    struct Builder {
        Builder() = default;
        explicit Builder(const unordered_map<DescriptorSetLayout*, uint32_t>& max_descriptor_sets) { addLayouts(max_descriptor_sets); }

        void addLayout(DescriptorSetLayout* layout, uint32_t alloc_count);
        void addLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations);

        void removeLayout(DescriptorSetLayout* layout, uint32_t amount = VK_WHOLE_SIZE);
        void removeLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations);

    private:
        unordered_map<DescriptorSetLayout*, uint32_t> maxDescriptorSets;

        friend DescriptorPool;
    };


    //TEMP left off here implement this
    /**
     * \param sets to allocate, the ptr will be stored
     * \return result of vkAllocateDescriptorSets()
     * \note descriptor sets are not required to stay valid
     * \note the pool does not take ownership of the sets
     * \note all already stored, unallocated descriptor sets will be allocated too
     */
    VkResult allocSets(const vector<DescriptorSet*>& sets);



    /**
     * \brief resets the pool -> resets all descriptor set slots
     * \param clear_sets remove all stored descriptor set pointers
     */
    void reset(bool clear_sets = true);
    /**
     * \param set must be an unallocated set
     */
    void removeDescriptorSet(DescriptorSet* set);
    /**
     * \brief
     * \param sets must be unallocated sets
     */
    void removeDescriptorSets(const vector<DescriptorSet*>& sets);

    /**
     * \brief removes all descriptor sets from the pool, pool must be reset before
     */
    void clear();

private:
    vector<VkDescriptorPoolSize> calcPoolSizes();

    /**
     * \throws cth::except::vk_result_exception data: VkResult of vkCreateDescriptorPool()
     */
    void create();

    void descriptorSetDestroyed(DescriptorSet* set);

    Device* device;
    uint32_t maxAllocatedSets;

    unordered_map<DescriptorSetLayout*, uint32_t> maxDescriptorSets{};
    unordered_map<DescriptorSetLayout*, uint32_t> descriptorSetUses{};

    unordered_set<DescriptorSet*> descriptorSets{};

    VkDescriptorPool vkPool = VK_NULL_HANDLE;

    bool _reset = true;

    friend DescriptorSet;

public:
    /**
    * \param max_descriptor_sets [layout, count] pairs -> limit for allocated sets per layout
    * \throws cth::except::vk_result_exception data: VkResult of vkCreateDescriptorPool()
    */
    DescriptorPool(Device* device, const Builder& max_descriptor_sets);
    ~DescriptorPool();

    [[nodiscard]] VkDescriptorPool get() const { return vkPool; }
};
} // namespace cth
