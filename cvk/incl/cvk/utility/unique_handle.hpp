#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>


struct VmaAllocator_T;

namespace cvk {
class Core;
class Instance;
}

namespace cvk::dev {
[[nodiscard]] DeviceTable device_table(Core const&);
[[nodiscard]] VkInstance vk_instance(Instance const&);
[[nodiscard]] VmaAllocator_T* vma_allocator(Core const&);
}

namespace cvk {
/**
 * @brief deleter of device objects, context: the core
 * @details calls T::destroy(core.deviceTable(), handle)
 * @pre the core outlives the deleter
 */
template<class T>
class core_deleter {
public:
    explicit core_deleter(Core const& core) : _core{&core} {}

    template<class Handle>
    void operator()(Handle handle) const { T::destroy(dev::device_table(*_core), handle); }

    [[nodiscard]] Core const& core() const { return *_core; }

private:
    not_null<Core const*> _core;
};

/**
 * @brief deleter of memory objects, context: the core
 * @details calls T::destroy(core.allocator().get(), handle)
 * @pre the core outlives the deleter
 */
template<class T>
class vma_deleter : public core_deleter<T> {
public:
    using core_deleter<T>::core_deleter;

    template<class Handle>
    void operator()(Handle handle) const { T::destroy(dev::vma_allocator(this->core()), handle); }
};

/**
 * @brief deleter of instance objects, context: the VkInstance
 * @details calls T::destroy(vkInstance, handle),
 *  stores the handle, not the Instance: the instance owns its messenger and can move
 */
template<class T>
class instance_deleter {
public:
    explicit instance_deleter(VkInstance vkInstance) : _vkInstance{vkInstance} {}
    explicit instance_deleter(Instance const& instance) : instance_deleter{dev::vk_instance(instance)} {}

    template<class Handle>
    void operator()(Handle handle) const { T::destroy(_vkInstance.get(), handle); }

    [[nodiscard]] VkInstance vkInstance() const { return _vkInstance.get(); }

private:
    vk_not_null<VkInstance> _vkInstance;
};

/**
 * @brief deleter without context, calls T::destroy(handle)
 */
template<class T>
struct handle_deleter {
    template<class Handle>
    void operator()(Handle handle) const { T::destroy(handle); }
};

/**
 * @brief owning Vulkan handle, std::unique_ptr with a deleter that holds the destroy context
 * @tparam Handle Vulkan handle, or Deleter::pointer for handles that are not pointers
 * @tparam Deleter @ref core_deleter, @ref vma_deleter, @ref instance_deleter, @ref handle_deleter or a deleter derived from them
 * @details a handle comes only together with its context: the constructors and reset() construct the deleter,
 *  a missing context argument does not compile,
 *  move assignment destroys the old handle with the old context, then takes handle and context
 */
template<class Handle, class Deleter>
class unique_handle {
    using ptr_t = std::unique_ptr<std::remove_pointer_t<Handle>, Deleter>;
    static_assert(std::same_as<typename ptr_t::pointer, Handle>, "Handle must be a pointer or Deleter::pointer");

public:
    using handle_t = Handle;
    using deleter_t = Deleter;

    /**
     * @brief owns no handle
     * @param context to construct the deleter
     */
    template<class... Args> requires std::constructible_from<Deleter, Args...>
    explicit unique_handle(Args&&... context) : _ptr{Handle{}, Deleter{std::forward<Args>(context)...}} {}

    /**
     * @brief owns @ref handle
     * @param context to construct the deleter
     */
    template<class... Args> requires std::constructible_from<Deleter, Args...>
    explicit unique_handle(Handle handle, Args&&... context) :
        _ptr{handle, Deleter{std::forward<Args>(context)...}} {}

    ~unique_handle() = default;

    /**
     * @brief destroys the owned handle, keeps the context
     */
    void reset() noexcept { _ptr.reset(); }

    /**
     * @brief destroys the owned handle with the old context, then owns @ref handle with the new context
     * @param context to construct the deleter
     */
    template<class... Args> requires std::constructible_from<Deleter, Args...>
    void reset(Handle handle, Args&&... context) noexcept {
        _ptr.reset();
        _ptr.get_deleter() = Deleter{std::forward<Args>(context)...};
        _ptr.reset(handle);
    }

    /**
     * @brief gives up the handle without destroying, keeps the context
     */
    [[nodiscard]] Handle release() noexcept { return _ptr.release(); }

    [[nodiscard]] Handle get() const noexcept { return _ptr.get(); }
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(_ptr); }

    [[nodiscard]] Deleter const& deleter() const noexcept { return _ptr.get_deleter(); }

    /**
     * @pre the deleter has core()
     */
    [[nodiscard]] decltype(auto) core() const requires requires(Deleter const& d) { d.core(); } {
        return deleter().core();
    }

    /**
     * @pre the deleter has vkInstance()
     */
    [[nodiscard]] VkInstance vkInstance() const requires requires(Deleter const& d) { d.vkInstance(); } {
        return deleter().vkInstance();
    }

private:
    ptr_t _ptr;

public:
    unique_handle(unique_handle const& other) = delete;
    unique_handle(unique_handle&& other) noexcept = default;
    unique_handle& operator=(unique_handle const& other) = delete;
    unique_handle& operator=(unique_handle&& other) noexcept = default;
};

/**
 * @brief @ref unique_handle of a device object, the deleter stores the core
 * @tparam T owner class with a static destroy(DeviceTable, Handle)
 */
template<class Handle, class T>
using core_handle = unique_handle<Handle, core_deleter<T>>;

/**
 * @brief @ref unique_handle of a memory object, the deleter stores the core and frees through its allocator
 * @tparam T owner class with a static destroy(VmaAllocator, Handle)
 */
template<class Handle, class T>
using vma_handle = unique_handle<Handle, vma_deleter<T>>;
}
