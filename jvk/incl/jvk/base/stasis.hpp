#pragma once
#include "jvk/data/resource_heap.hpp"
#include "jvk/data/sampler_heap.hpp"

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
 * @details
 * - destroys the pooled handles first, the rest in the order of `add()`
 * - `clear()` with a position of `end()`: destroys the handles before it, keeps the later ones
 * - heap cells: `clear()` erases them, see `ResourceHeap::Cell`, `SamplerHeap::Cell`
 * @attention not thread safe
 */
class Stasis {
public:
    using handle_t = std::variant<
        VkSemaphore,
        VkFence,
        VkImage,
        VkImageView,
        VkBuffer,
        VkShaderModule,
        VkPipeline,
        VkCommandPool,
        VkSwapchainKHR,
        VmaAllocation,
        ResourceHeap::Cell<cvk::SampledImageDescriptor>,
        ResourceHeap::Cell<cvk::StorageImageDescriptor>,
        ResourceHeap::Cell<cvk::InputAttachmentDescriptor>,
        SamplerHeap::Cell>;

    using pooled_t = std::variant<std::tuple<VkCommandPool, VkCommandBuffer>>;

    /**
     * stable position in the added handles
     * @details counts every handle added since the construction, a `clear()` does not move it
     */
    struct pos {
        size_t handles;
        size_t pooledHandles;
    };

    /**
     * takes the device table and the allocator of the core
     * @pre the core outlives the handles
     */
    explicit Stasis(cvk::Core const&);

    Stasis(cvk::DeviceTable table, VmaAllocator vmaAllocator);

    ~Stasis() { clear(); }

    /**
     * takes the raw handles of the state
     * @pre the gpu does not use the handles after the next `clear()`
     */
    template<class State> requires(!std::is_lvalue_reference_v<State>) && requires(State&& state) {
        std::move(state).decompose();
    }
    void add(State&& state);

    /**
     * destroys the handles added before the position, keeps the capacity
     * @details order independent: a later position includes the earlier ones
     * @pre `pos` comes from `end()` of this stasis
     * @pre the gpu does not use the handles before the position
     */
    void clear(pos const&);

    /**
     * destroys the handles, keeps the capacity
     * @pre the gpu does not use the handles
     */
    void clear() { clear(end()); }

    /**
     * @return position after the last `add()`
     */
    [[nodiscard]] constexpr pos end() const {
        return pos{
            .handles = _clearedHandles + _handles.size(),
            .pooledHandles = _clearedPooledHandles + _pooledHandles.size(),
        };
    }

    [[nodiscard]] constexpr size_t size() const { return _handles.size() + _pooledHandles.size(); }

    [[nodiscard]] constexpr bool empty() const {
        return _handles.empty() && _pooledHandles.empty();
    }

    [[nodiscard]] constexpr cvk::DeviceTable table() const { return _table; }
    [[nodiscard]] constexpr VmaAllocator allocator() const { return _vmaAllocator; }

private:
    template<class Grave>
    void push(Grave grave);

    cvk::DeviceTable _table;
    VmaAllocator _vmaAllocator;

    std::vector<handle_t> _handles{};
    std::vector<pooled_t> _pooledHandles{};

    /** handles destroyed by `clear()`, the position of `_handles.front()` */
    size_t _clearedHandles = 0;
    /** pooled handles destroyed by `clear()`, the position of the first one */
    size_t _clearedPooledHandles = 0;

public:
    Stasis(Stasis const& other) = delete;
    Stasis(Stasis&& other) noexcept = default;
    Stasis& operator=(Stasis const& other) = delete;

    /**
     * destroys the handles of this, then takes the handles, the positions and the contexts of other
     * @details the positions of `end()` before the assignment do not apply after it
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
template<class State> requires(!std::is_lvalue_reference_v<State>) && requires(State&& state) {
    std::move(state).decompose();
}
void Stasis::add(State&& state) {
    std::apply([this](auto... graves) { (push(graves), ...); }, std::move(state).decompose());
}

template<class Grave>
void Stasis::push(Grave grave) {
    if constexpr(dev::is_optional<Grave>) {
        if(grave.has_value())
            push(*grave);
    } else if constexpr(dev::alternative_of<Grave,
        pooled_t>) { _pooledHandles.emplace_back(grave); } else {
        static_assert(
            dev::alternative_of<Grave, handle_t>,
            "the stasis has no alternative for the handle"
        );
        _handles.emplace_back(grave);
    }
}
}
