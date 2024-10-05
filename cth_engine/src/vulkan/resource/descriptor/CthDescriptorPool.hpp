#pragma once

#include "vulkan/utility/cth_constants.hpp"

#include <cth/pointers.hpp>
#include <cth/io/log.hpp>

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>



namespace cth::vk {
class Core;
class DescriptorSet;
class DescriptorSetLayout;


/**
 * @brief wrapper class for the VkDescriptorPool
 * @note the pool allocates the max_descriptor_sets instantly
 * @note all DescriptorSetLayout's ever used with the pool must be known at its creation
 */
class DescriptorPool {
public:
    struct Builder;
    /**
    * @param builder [layout, count] pairs -> limit for allocated sets per layout
    * @throws cth::vk::result_exception data: VkResult of vkCreateDescriptorPool()
    */
    DescriptorPool(cth::not_null<Core const*> device, Builder const& builder);
    ~DescriptorPool();
    /**
     * @note descriptor sets are not required to stay valid
     * @note the pool does not take ownership of the sets
     */
    void writeSets(std::vector<DescriptorSet*> const& sets);



    /**
     * @brief resets the pool -> resets all descriptor sets
     * @throws cth::vk::result_exception data: VkResult of vkResetDescriptorPool()
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

        std::span<VkDescriptorSet> span{};
        uint32_t used = 0;
    };

    std::vector<VkDescriptorPoolSize> calcPoolSizes();

    void initSetEntries(Builder const& builder);
    /**
     * @throws cth::vk::result_exception data: VkResult of vkCreateDescriptorPool()
     */
    void create();

    /**
     * @throws cth::vk::result_exception data: VkResult of vkAllocateDescriptorSets()
     */
    void allocSets();

    void returnSet(DescriptorSet* set);

    cth::not_null<Core const*> _core;

    std::unordered_map<DescriptorSetLayout const*, SetLayoutEntry> _allocatedSets{};
    std::vector<VkDescriptorSet> _vkSets{};

    std::unordered_set<DescriptorSet*> _descriptorSets{};

    move_ptr<VkDescriptorPool_T> _handle = VK_NULL_HANDLE;

    bool _reset = true;

    friend DescriptorSet;

public:
    [[nodiscard]] VkDescriptorPool get() const { return _handle.get(); }

    struct Builder {
        Builder() = default;
        explicit Builder(std::unordered_map<DescriptorSetLayout const*, uint32_t> const& max_descriptor_sets) { addLayouts(max_descriptor_sets); }

        void addLayout(DescriptorSetLayout const* layout, uint32_t alloc_count);
        void addLayouts(std::unordered_map<DescriptorSetLayout const*, uint32_t> const& set_allocations);

        void removeLayout(DescriptorSetLayout const* layout, size_t amount = constants::WHOLE_SIZE);
        void removeLayouts(std::unordered_map<DescriptorSetLayout const*, uint32_t> const& set_allocations);

    private:
        std::unordered_map<DescriptorSetLayout const*, size_t> _maxDescriptorSets;

        friend DescriptorPool;
    };

    DescriptorPool(DescriptorPool const& other) = delete;
    DescriptorPool(DescriptorPool&& other) = delete;
    DescriptorPool& operator=(DescriptorPool const& other) = delete;
    DescriptorPool& operator=(DescriptorPool&& other) = delete;
};
} // namespace cth
