#pragma once
#include <vulkan/vulkan.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cth/cth_log.hpp>

#include "vulkan/utility/CthConstants.hpp"

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
    struct Builder;
    /**
    * \param builder [layout, count] pairs -> limit for allocated sets per layout
    * \throws cth::except::vk_result_exception data: VkResult of vkCreateDescriptorPool()
    */
    DescriptorPool(Device* device, const Builder& builder);
    ~DescriptorPool();
    /**
     * \note descriptor sets are not required to stay valid
     * \note the pool does not take ownership of the sets
     */
    void writeSets(const vector<DescriptorSet*>& sets);



    /**
     * \brief resets the pool -> resets all descriptor sets
     * \throws cth::except::vk_result_exception data: VkResult of vkResetDescriptorPool()
     */
    void reset();

private:
    struct SetLayoutEntry {
        void reset() { used = 0; }
        [[nodiscard]] VkDescriptorSet newVkSet() {
            CTH_ERR(used >= span.size(), "out of descriptor sets") throw details->exception();
            return span[used++];
        }
        [[nodiscard]] size_t size() const { return span.size(); }

        span<VkDescriptorSet> span{};
        uint32_t used = 0;
    };

    vector<VkDescriptorPoolSize> calcPoolSizes();

    void initSetEntries(const Builder& builder);
    /**
     * \throws cth::except::vk_result_exception data: VkResult of vkCreateDescriptorPool()
     */
    void create();

    /**
     * \throws cth::except::vk_result_exception data: VkResult of vkAllocateDescriptorSets()
     */
    void allocSets();

    void descriptorSetDestroyed(DescriptorSet* set);

    Device* device;

    unordered_map<DescriptorSetLayout*, SetLayoutEntry> allocatedSets{};
    vector<VkDescriptorSet> vkSets{};

    unordered_set<DescriptorSet*> descriptorSets{};

    VkDescriptorPool vkPool = VK_NULL_HANDLE;

    bool reset_ = true;

    friend DescriptorSet;

public:
    [[nodiscard]] VkDescriptorPool get() const { return vkPool; }

    struct Builder {
        Builder() = default;
        explicit Builder(const unordered_map<DescriptorSetLayout*, uint32_t>& max_descriptor_sets) { addLayouts(max_descriptor_sets); }

        void addLayout(DescriptorSetLayout* layout, uint32_t alloc_count);
        void addLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations);

        void removeLayout(DescriptorSetLayout* layout, size_t amount = Constant::WHOLE_SIZE);
        void removeLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations);

    private:
        unordered_map<DescriptorSetLayout*, size_t> maxDescriptorSets;

        friend DescriptorPool;
    };

    DescriptorPool(const DescriptorPool& other) = delete;
    DescriptorPool(DescriptorPool&& other) = delete;
    DescriptorPool& operator=(const DescriptorPool& other) = delete;
    DescriptorPool& operator=(DescriptorPool&& other) = delete;
};
} // namespace cth
