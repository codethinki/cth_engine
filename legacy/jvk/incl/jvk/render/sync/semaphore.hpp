#pragma once
#include "jvk/base/device_table.hpp"
#include "jvk/utility/constants.hpp"
#include "jvk/utility/types.hpp"

#include <volk.h>

#include <cth/ptr.hpp>

namespace jvk {
class Core;


class Semaphore {
public:
    struct State;

    /**
     * @brief base constructor
     */
    explicit Semaphore(Core const& core);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref Semaphore::Semaphore(not_null<Core const*>)
     */
    Semaphore(Core const& core, State const& state);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref Semaphore::Semaphore(not_null<Core const*>)
     */
    explicit Semaphore(Core const& core, create_t);

    virtual ~Semaphore() { optDestroy(); }

    /**
     * @brief wraps the @ref State
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief creates the semaphore
     * @note calls @ref optDestroy()
     * @throws jvk::result_exception result of @ref vkCreateSemaphore()
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

    static void destroy(DeviceTable table, VkSemaphore vk_semaphore);

protected:
    virtual VkSemaphoreCreateInfo createInfo();
    /**
     * @throws jvk::result_exception result of vkCreateSemaphore()
     */
    virtual void createHandle(VkSemaphoreCreateInfo const& info);
    virtual void reset();

    not_null<Core const*> _core;

private:
    move_ptr<VkSemaphore_T> _handle{};

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }

    Semaphore(Semaphore const& other) = default;
    Semaphore(Semaphore&& other) = default;
    Semaphore& operator=(Semaphore const& other) = default;
    Semaphore& operator=(Semaphore&& other) = default;


    static void debug_check(Semaphore const& semaphore);
};

}

namespace jvk {
struct Semaphore::State {
    jvk::vk_not_null<VkSemaphore> vkSemaphore;
};
}

//debug checks

namespace jvk {
inline void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}

}
