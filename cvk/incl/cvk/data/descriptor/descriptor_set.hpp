#pragma once
#include "descriptor_pool.hpp"
#include "descriptor_write.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/handle/destroy.hpp"
#include "cvk/utility/handle/unique_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <initializer_list>
#include <span>
#include <tuple>


namespace cvk {
class Core;

/**
 * VkDescriptorSet wrapper
 * @details allocated from its pool with its layout
 * @details @ref DescriptorPool::reset() frees the set, @ref created() is false after it
 */
class DescriptorSet {
public:
    struct State;

    /**
     * base constructor
     * @param variableCount descriptors of the variable count binding (0 => its maximum count)
     * @pre @ref DescriptorPool::created(), @ref DescriptorSetLayout::created()
     * @pre variableCount != 0 -> the layout has a variable count binding with a maximum >= variableCount
     */
    DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t variableCount = 0);

    /**
     * constructs and wraps
     * @details calls @ref DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t), @ref wrap()
     */
    DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t variableCount, State const&);

    /**
     * constructs and creates
     * @details calls @ref DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t), @ref create()
     */
    explicit DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t variableCount, create_t);

    /**
     * constructs and creates with the maximum variable count
     */
    explicit DescriptorSet(DescriptorPool const& pool, DescriptorSetLayout const& layout, create_t) : DescriptorSet{
        pool,
        layout,
        0,
        cvk::create
    } {}

    /**
     * @note calls @ref optDestroy()
     */
    ~DescriptorSet() { optDestroy(); }

    /**
     * wraps the state
     * @pre @ref State::vkPool is the pool of the set
     * @pre the pool allocated the set with the layout and the variable count
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * allocates the set from the pool
     * @pre @ref DescriptorPool::created()
     * @pre update after bind layout -> the pool has VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkAllocateDescriptorSets()
     */
    void create();

    /**
     * destroys and forgets
     * @details the pool frees no sets -> the memory returns with @ref DescriptorPool::reset()
     * @pre @ref created()
     * @pre the gpu does not use the set
     */
    void destroy();

    /**
     * if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * releases ownership, returns the state and forgets
     * @pre @ref created()
     * @pre the pool was not reset since the allocation of the set
     */
    State release();

    /**
     * writes the descriptors into the set with one vkUpdateDescriptorSets()
     * @pre @ref created()
     * @pre the bindings exist, the kind of each write matches the binding type
     * @pre the elements lie in the bindings, the buffer ranges in the buffers
     * @pre the gpu does not use the written descriptors, except with VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
     * @attention texel buffer, inline uniform block and acceleration structure descriptors are not supported
     */
    void write(std::span<DescriptorWrite const>) const;

    /**
     * writes the descriptors into the set, see @ref write(std::span<DescriptorWrite const>) const
     */
    void write(std::initializer_list<DescriptorWrite> writes) const { write(std::span{writes.begin(), writes.size()}); }

    /**
     * writes one descriptor into the set, see @ref write(std::span<DescriptorWrite const>) const
     */
    void write(DescriptorWrite const& descriptorWrite) const { write(std::span{&descriptorWrite, 1}); }

    /**
     * @return the set is allocated and its pool was not reset since
     */
    [[nodiscard]] constexpr bool created() const {
        return static_cast<bool>(_handle) && _handle.deleter().resets() == pool().resets();
    }
    [[nodiscard]] constexpr VkDescriptorSet get() const { return _handle.get(); }
    [[nodiscard]] constexpr DescriptorPool const& pool() const { return _handle.deleter().pool(); }
    [[nodiscard]] constexpr Core const& core() const { return pool().core(); }
    [[nodiscard]] constexpr DescriptorSetLayout const& layout() const { return *_layoutPtr; }

    /**
     * @return descriptors of the variable count binding, 0 -> its maximum count or no such binding
     */
    [[nodiscard]] constexpr uint32_t variableCount() const { return _variableCount; }

private:
    /**
     * frees through the pool, nothing if the pool was reset since the allocation
     */
    class pool_deleter {
    public:
        constexpr explicit pool_deleter(DescriptorPool const& pool) : _poolPtr{&pool}, _resets{pool.resets()} {}

        void operator()(VkDescriptorSet) const;

        [[nodiscard]] constexpr DescriptorPool const& pool() const { return *_poolPtr; }

        /**
         * @return resets of the pool at the allocation
         */
        [[nodiscard]] constexpr uint64_t resets() const { return _resets; }

    private:
        not_null_ptr<DescriptorPool const> _poolPtr;
        uint64_t _resets;
    };

    /**
     * @return descriptor count of the binding, the variable count for the variable count binding
     */
    [[nodiscard]] constexpr uint32_t capacity(VkDescriptorSetLayoutBinding const&) const;

    void forget();

    unique_handle<VkDescriptorSet, pool_deleter> _handle;
    not_null_ptr<DescriptorSetLayout const> _layoutPtr;
    uint32_t _variableCount;

public:
    DescriptorSet(DescriptorSet const& other) = delete;
    DescriptorSet(DescriptorSet&& other) noexcept = default;
    DescriptorSet& operator=(DescriptorSet const& other) = delete;
    DescriptorSet& operator=(DescriptorSet&& other) noexcept = default;

    static constexpr void debug_check(DescriptorSet const&);
};
}

namespace cvk {
struct DescriptorSet::State {
    /**
     * pool that allocated @ref vkSet
     */
    vk_not_null<VkDescriptorPool> vkPool;
    vk_not_null<VkDescriptorSet> vkSet;

    /**
     * the raw handles to destroy, in destruction order
     * @pre the pool has VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
     */
    [[nodiscard]] constexpr std::tuple<std::tuple<VkDescriptorPool, VkDescriptorSet>> decompose() && {
        return {std::tuple{vkPool.get(), vkSet.get()}};
    }
};
}

namespace cvk {
constexpr uint32_t DescriptorSet::capacity(VkDescriptorSetLayoutBinding const& binding) const {
    return &binding == _layoutPtr->variableBinding() && _variableCount != 0 ? _variableCount : binding.descriptorCount;
}

constexpr void DescriptorSet::debug_check(DescriptorSet const& set) {
    CTH_CRITICAL(!set.created(), "descriptor set must be created, a pool reset frees it") {}
}
}
