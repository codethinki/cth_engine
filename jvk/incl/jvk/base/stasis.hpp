#pragma once
#include "cvk/base/device_table.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <cth/meta/concepts.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace cvk {
class Core;
}

namespace jvk {
/**
 * raw handles of a device, held until they are destroyed, move only
 * @details @ref add() takes the handles of a cvk state through its decompose(),
 *  @ref clear() and the destructor destroy them with cvk::destroy(): the pool children first, the rest in the order of
 *  @ref add(), the context of a handle is the device table or the allocator, the one that cvk::destroy() takes
 * @attention not thread safe
 */
class Stasis {
public:
    /**
     * the handles that are destroyed on their own
     */
    using handle_t = std::variant<
        VkSemaphore,
        VkFence,
        VkImage,
        VkImageView,
        VkSampler,
        VkBuffer,
        VkDescriptorPool,
        VkDescriptorSetLayout,
        VkPipelineLayout,
        VkShaderModule,
        VkPipeline,
        VkCommandPool,
        VkSwapchainKHR,
        VmaAllocation>;

    /**
     * the handles that their pool frees
     */
    using pool_child_t = std::variant<
        std::tuple<VkCommandPool, VkCommandBuffer>,
        //TODO review pools without VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT and pool resets
        std::tuple<VkDescriptorPool, VkDescriptorSet>>;

    /**
     * takes the device table and the allocator of the core
     * @pre @ref cvk::Core::created()
     * @pre the core outlives the handles of the stasis
     */
    explicit Stasis(cvk::Core const&);

    /**
     * @param table of the device of the handles
     * @param vmaAllocator of the allocations
     */
    Stasis(cvk::DeviceTable table, VmaAllocator vmaAllocator);

    /**
     * @note calls @ref clear()
     */
    ~Stasis() { clear(); }

    /**
     * takes the raw handles of the state
     * @details calls decompose(), skips the empty std::optional parts, the pool children go first at @ref clear()
     * @param state of a cvk object of the device, e.g. from release()
     * @pre the gpu does not use the handles after the next @ref clear()
     */
    template<class State>
    requires(!std::is_lvalue_reference_v<State>) && requires(State&& state) { std::move(state).decompose(); }
    void add(State&& state);

    /**
     * destroys the handles, the pool children first, keeps the capacity
     * @pre the gpu does not use the handles
     */
    void clear();

    /**
     * @return number of handles and pool children
     */
    [[nodiscard]] constexpr size_t size() const { return _handles.size() + _poolChildren.size(); }

    [[nodiscard]] constexpr bool empty() const { return _handles.empty() && _poolChildren.empty(); }

    [[nodiscard]] constexpr cvk::DeviceTable table() const { return _table; }
    [[nodiscard]] constexpr VmaAllocator allocator() const { return _allocator; }

private:
    /**
     * stores one part of a decomposed state
     */
    template<class Grave>
    void push(Grave grave);

    cvk::DeviceTable _table;
    VmaAllocator _allocator;

    std::vector<handle_t> _handles{};
    std::vector<pool_child_t> _poolChildren{};

public:
    Stasis(Stasis const& other) = delete;
    Stasis(Stasis&& other) noexcept = default;
    Stasis& operator=(Stasis const& other) = delete;

    /**
     * destroys the handles of this, then takes the handles and the contexts of other
     * @note calls @ref clear()
     */
    Stasis& operator=(Stasis&& other) noexcept;
};
}

namespace jvk::dev {
template<class T, class Variant>
constexpr bool alternative_of = false;

template<class T, class... Ts>
constexpr bool alternative_of<T, std::variant<Ts...>> = cth::mta::is_any_of<T, Ts...>;

template<class T>
constexpr bool is_optional = false;

template<class T>
constexpr bool is_optional<std::optional<T>> = true;
}

namespace jvk {
template<class State>
requires(!std::is_lvalue_reference_v<State>) && requires(State&& state) { std::move(state).decompose(); }
void Stasis::add(State&& state) {
    std::apply([this](auto... graves) { (push(graves), ...); }, std::move(state).decompose());
}

template<class Grave>
void Stasis::push(Grave grave) {
    if constexpr(dev::is_optional<Grave>) {
        if(grave.has_value()) push(*grave);
    } else if constexpr(dev::alternative_of<Grave, pool_child_t>) {
        _poolChildren.emplace_back(grave);
    } else {
        static_assert(dev::alternative_of<Grave, handle_t>, "the stasis has no alternative for the handle");
        _handles.emplace_back(grave);
    }
}
}
