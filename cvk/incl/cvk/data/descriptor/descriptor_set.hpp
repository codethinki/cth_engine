#pragma once
#include "descriptor_pool.hpp"
#include "descriptor_write.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/types.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <span>


namespace cvk {
class BaseCmdBuffer;
class Core;
class PipelineLayout;

/**
 * @brief VkDescriptorSet wrapper, allocated from its pool with its layout
 * @details @ref DescriptorPool::reset() frees the set, @ref created() is false after it
 */
class DescriptorSet {
public:
    using vk_handle_t = VkDescriptorSet;
    struct State;

    /**
     * @brief base constructor
     * @param variableCount descriptors of the variable count binding (0 => its maximum count)
     * @pre @ref DescriptorPool::created(), @ref DescriptorSetLayout::created()
     * @pre variableCount != 0 -> the layout has a variable count binding with a maximum >= variableCount
     */
    DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t variableCount = 0);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t)
     * - @ref wrap()
     */
    DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t variableCount, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t)
     * - @ref create()
     */
    DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t variableCount, create_t);

    /**
     * @brief constructs and creates with the maximum variable count
     * @details calls @ref DescriptorSet(DescriptorPool const&, DescriptorSetLayout const&, uint32_t, create_t)
     */
    DescriptorSet(DescriptorPool const& pool, DescriptorSetLayout const& layout, create_t) : DescriptorSet{
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
     * @brief wraps the state
     * @pre the pool allocated the set with the layout and the variable count
     * @post @ref created()
     * @note calls @ref optDestroy()
     */
    void wrap(State const&);

    /**
     * @brief allocates the set from the pool
     * @pre @ref DescriptorPool::created()
     * @pre update after bind layout -> the pool has VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkAllocateDescriptorSets()
     */
    void create();

    /**
     * @brief destroys and forgets
     * @details the pool frees sets -> vkFreeDescriptorSets(), else the memory returns with @ref DescriptorPool::reset()
     * @pre @ref created()
     * @pre the gpu does not use the set
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership, returns the state and forgets
     * @pre @ref created()
     */
    State release();

    /**
     * @brief writes the descriptors into the set
     * @pre @ref created()
     * @pre the bindings exist, the descriptors match their types and lie in them
     * @pre the gpu does not use the written descriptors, except with VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
     * @attention texel buffer, inline uniform block and acceleration structure descriptors are not supported
     */
    void write(std::span<DescriptorWrite const>) const;

    /**
     * @brief records the bind of the set
     * @param setIndex set number in the pipeline layout
     * @param dynamicOffsets one per dynamic buffer descriptor of the set
     * @pre @ref created()
     * @pre @ref BaseCmdBuffer::recording()
     * @pre the set layout of setIndex in the pipeline layout is compatible with the layout of the set
     */
    void bind(
        BaseCmdBuffer const&,
        PipelineLayout const&,
        uint32_t setIndex,
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        std::span<uint32_t const> dynamicOffsets = {}
    ) const;

    static void destroy(DeviceTable table, VkDescriptorPool, VkDescriptorSet);

    /**
     * @return the set is allocated and its pool was not reset since
     */
    [[nodiscard]] constexpr bool created() const {
        return static_cast<bool>(_handle) && _handle.deleter().resets() == pool().resets();
    }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr DescriptorPool const& pool() const { return _handle.deleter().pool(); }
    [[nodiscard]] constexpr Core const& core() const { return pool().core(); }
    [[nodiscard]] constexpr DescriptorSetLayout const& layout() const { return *_layout; }

    /**
     * @return descriptors of the variable count binding, 0 -> its maximum count or no such binding
     */
    [[nodiscard]] constexpr uint32_t variableCount() const { return _variableCount; }

private:
    /**
     * @brief frees through the pool, nothing if the pool was reset since the allocation
     */
    class pool_deleter {
    public:
        constexpr explicit pool_deleter(DescriptorPool const& pool) : _pool{&pool}, _resets{pool.resets()} {}

        void operator()(VkDescriptorSet) const;

        [[nodiscard]] constexpr DescriptorPool const& pool() const { return *_pool; }

        /**
         * @return resets of the pool at the allocation
         */
        [[nodiscard]] constexpr uint64_t resets() const { return _resets; }

    private:
        not_null_ptr<DescriptorPool const> _pool;
        uint64_t _resets;
    };

    /**
     * @return descriptor count of the binding, the variable count for the variable count binding
     */
    [[nodiscard]] constexpr uint32_t capacity(VkDescriptorSetLayoutBinding const&) const;

    void forget();

    unique_handle<vk_handle_t, pool_deleter> _handle;
    not_null_ptr<DescriptorSetLayout const> _layout;
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
    vk_not_null<VkDescriptorSet> vkSet;
};
}

namespace cvk {
constexpr uint32_t DescriptorSet::capacity(VkDescriptorSetLayoutBinding const& binding) const {
    return &binding == _layout->variableBinding() && _variableCount != 0 ? _variableCount : binding.descriptorCount;
}

constexpr void DescriptorSet::debug_check(DescriptorSet const& set) {
    CTH_CRITICAL(!set.created(), "descriptor set must be created, a pool reset frees it") {}
}
}
