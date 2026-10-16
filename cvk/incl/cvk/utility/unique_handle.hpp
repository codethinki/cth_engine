#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>


namespace cvk {
class Core;
class Instance;
}

namespace cvk::dev {
[[nodiscard]] DeviceTable device_table(Core const& core);
[[nodiscard]] VkInstance vk_instance(Instance const& instance);
}

namespace cvk {
/**
 * @brief T::destroy(DeviceTable, Handle) exists
 */
template<class T, class Handle>
concept core_destroyable = requires(DeviceTable table, Handle handle) { T::destroy(table, handle); };

/**
 * @brief T::destroy(VkInstance, Handle) exists
 */
template<class T, class Handle>
concept instance_destroyable = requires(VkInstance vkInstance, Handle handle) { T::destroy(vkInstance, handle); };

/**
 * @brief T::destroy(Handle) exists
 */
template<class T, class Handle>
concept handle_destroyable = requires(Handle handle) { T::destroy(handle); };


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

namespace dev {
    /**
     * @brief T::destroy fits no default deleter
     */
    struct no_default_deleter {};
}

/**
 * @brief deleter that fits the static destroy of T
 * @details first match:
 * - T::destroy(DeviceTable, Handle) -> @ref core_deleter
 * - T::destroy(VkInstance, Handle) -> @ref instance_deleter
 * - T::destroy(Handle) -> @ref handle_deleter
 * @attention inside T: declare T::destroy before the member that uses this, else no deleter fits
 */
template<class T, class Handle>
using default_deleter_t = std::conditional_t<
    core_destroyable<T, Handle>,
    core_deleter<T>,
    std::conditional_t<
        instance_destroyable<T, Handle>,
        instance_deleter<T>,
        std::conditional_t<handle_destroyable<T, Handle>, handle_deleter<T>, dev::no_default_deleter>
    >
>;


/**
 * @brief owning Vulkan handle, std::unique_ptr with a deleter that holds the destroy context
 * @tparam T owner class, its static destroy picks the default deleter
 * @tparam Handle Vulkan handle, or Deleter::pointer for handles that are not pointers
 * @tparam Deleter custom deleters derive from @ref core_deleter, @ref instance_deleter or @ref handle_deleter
 * @details a handle comes only together with its context: the constructors and reset() construct the deleter,
 *  a missing context argument does not compile,
 *  move assignment destroys the old handle with the old context, then takes handle and context
 */
template<class T, class Handle, class Deleter = default_deleter_t<T, Handle>>
class unique_handle {
    static_assert(
        !std::same_as<Deleter, dev::no_default_deleter>,
        "T::destroy fits no default deleter: declare it before the member or supply a deleter"
    );

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
}
