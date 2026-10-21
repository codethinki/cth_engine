#pragma once
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>


namespace cvk {
/**
 * @brief T names the Vulkan handle it holds: using vk_handle_t = VkX
 */
template<class T>
concept vk_handle_owner = requires { typename T::vk_handle_t; };

/**
 * @brief deleter without context, calls T::destroy(handle)
 */
template<vk_handle_owner T>
struct handle_deleter {
    template<class Handle>
    constexpr void operator()(Handle handle) const { T::destroy(handle); }
};

/**
 * @brief owning Vulkan handle, std::unique_ptr with a deleter that holds the destroy context
 * @tparam Handle Vulkan handle, or Deleter::pointer for handles that are not pointers
 * @tparam Deleter @ref core_deleter, @ref vma_deleter, @ref instance_deleter, @ref handle_deleter,
 *  a deleter derived from them or one of its own
 * @details a handle comes only together with its context: the constructors and reset() construct the deleter,
 *  a missing context argument does not compile,
 *  move assignment destroys the old handle with the old context, then takes handle and context,
 *  the size is the size of the handle plus the size of the context
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
    constexpr explicit unique_handle(Args&&... context) : _ptr{Handle{}, Deleter{std::forward<Args>(context)...}} {}

    /**
     * @brief owns @ref handle
     * @param context to construct the deleter
     */
    template<class... Args> requires std::constructible_from<Deleter, Args...>
    constexpr explicit unique_handle(Handle handle, Args&&... context) :
        _ptr{handle, Deleter{std::forward<Args>(context)...}} {}

    constexpr ~unique_handle() = default;

    /**
     * @brief destroys the owned handle, keeps the context
     */
    constexpr void reset() noexcept { _ptr.reset(); }

    /**
     * @brief destroys the owned handle with the old context, then owns @ref handle with the new context
     * @param context to construct the deleter
     */
    template<class... Args> requires std::constructible_from<Deleter, Args...>
    constexpr void reset(Handle handle, Args&&... context) noexcept {
        _ptr.reset();
        _ptr.get_deleter() = Deleter{std::forward<Args>(context)...};
        _ptr.reset(handle);
    }

    /**
     * @brief gives up the handle without destroying, keeps the context
     */
    [[nodiscard]] constexpr Handle release() noexcept { return _ptr.release(); }

    [[nodiscard]] constexpr Handle get() const noexcept { return _ptr.get(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return static_cast<bool>(_ptr); }

    [[nodiscard]] constexpr Deleter const& deleter() const noexcept { return _ptr.get_deleter(); }

    /**
     * @pre the deleter has core()
     */
    [[nodiscard]] constexpr decltype(auto) core() const requires requires(Deleter const& d) { d.core(); } {
        return deleter().core();
    }

    /**
     * @pre the deleter has vkInstance()
     */
    [[nodiscard]] constexpr VkInstance vkInstance() const requires requires(Deleter const& d) { d.vkInstance(); } {
        return deleter().vkInstance();
    }

private:
    ptr_t _ptr;

public:
    unique_handle(unique_handle const& other) = delete;
    constexpr unique_handle(unique_handle&& other) noexcept = default;
    unique_handle& operator=(unique_handle const& other) = delete;
    constexpr unique_handle& operator=(unique_handle&& other) noexcept = default;
};
}
