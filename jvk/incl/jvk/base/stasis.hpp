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
 * raw device handles to destroy
 * @details destroys the pool children first, the rest in the order of @ref add()
 * @attention not thread safe
 */
class Stasis {
public:
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

    using pool_child_t = std::variant<
        std::tuple<VkCommandPool, VkCommandBuffer>,
        //TODO review pools without VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT and pool resets
        std::tuple<VkDescriptorPool, VkDescriptorSet>>;

    /**
     * takes the device table and the allocator of the core
     * @pre the core outlives the handles
     */
    explicit Stasis(cvk::Core const&);

    Stasis(cvk::DeviceTable table, VmaAllocator vmaAllocator);

    ~Stasis() { clear(); }

    /**
     * takes the raw handles of the state
     * @pre the gpu does not use the handles after the next @ref clear()
     */
    template<class State>
    requires(!std::is_lvalue_reference_v<State>) && requires(State&& state) { std::move(state).decompose(); }
    void add(State&& state);

    /**
     * destroys the handles, keeps the capacity
     * @pre the gpu does not use the handles
     */
    void clear();

    [[nodiscard]] constexpr size_t size() const { return _handles.size() + _poolChildren.size(); }

    [[nodiscard]] constexpr bool empty() const { return _handles.empty() && _poolChildren.empty(); }

    [[nodiscard]] constexpr cvk::DeviceTable table() const { return _table; }
    [[nodiscard]] constexpr VmaAllocator allocator() const { return _allocator; }

private:
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
     * destroys the handles of this, then takes the handles and contexts of other
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
