#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <volk.h>

#include <cth/pointers.hpp>

namespace cth::vk {
class Core;


class Semaphore {
public:
    struct State;

    /**
     * @brief base constructor
     */
    explicit Semaphore(cth::not_null<Core const*> core);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref Semaphore::Semaphore(not_null<Core const*>)
     */
    Semaphore(cth::not_null<Core const*> core, State const& state);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref Semaphore::Semaphore(not_null<Core const*>)
     */
    explicit Semaphore(cth::not_null<Core const*> core, bool create);

    virtual ~Semaphore() { optDestroy(); }

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the semaphore
     * @note calls @ref optDestroy()
     * @throws vk::result_exception result of @ref vkCreateSemaphore()
     */
    void create();

    /**
     * @brief destroys and resets
     * @attention requires @ref created()
     * @note submits to Core::destructionQueue() if available
     * @note calls @ref destroy(VkDevice, VkSemaphore)
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @attention requires @ref created()
     */
    // ReSharper disable once CppHiddenFunction
    State release();


    static void destroy(vk::not_null<VkDevice> vk_device, VkSemaphore vk_semaphore);

protected:
    virtual VkSemaphoreCreateInfo createInfo();
    /**
     * @throws cth::vk::result_exception result of vkCreateSemaphore()
     */
    virtual void createHandle(VkSemaphoreCreateInfo const& info);
    virtual void reset();

    cth::not_null<Core const*> _core;

private:
    move_ptr<VkSemaphore_T> _handle{};

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }

    Semaphore(Semaphore const& other) = default;
    Semaphore(Semaphore&& other) = default;
    Semaphore& operator=(Semaphore const& other) = default;
    Semaphore& operator=(Semaphore&& other) = default;


    static void debug_check(cth::not_null<Semaphore const*> semaphore);
};

}

namespace cth::vk {
struct Semaphore::State {
    vk::not_null<VkSemaphore> vkSemaphore;
};
}

//debug checks

namespace cth::vk {
inline void Semaphore::debug_check(cth::not_null<Semaphore const*> semaphore) {
    CTH_CRITICAL(!semaphore->created(), "semaphore must be created") {}
}

}
