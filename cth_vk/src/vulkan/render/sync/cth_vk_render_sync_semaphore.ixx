module;
#include "lib/volk.hpp"

#include <cth/io/io_log.hpp>

export module cth.vk.render.sync.semaphore;

import cth.vk.base.device_table;
import cth.vk.constants;
import cth.vk.util.types;
import cth.vk.base.core;

import cth.ptr;
import cth.io.log;

export namespace cth::vk {

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

    static void destroy(DeviceTable table, VkSemaphore vk_semaphore);

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


    static void debug_check(Semaphore const& semaphore);
};

}

export namespace cth::vk {
struct Semaphore::State {
    vk::not_null<VkSemaphore> vkSemaphore;
};
}

//debug checks

export namespace cth::vk {
void Semaphore::debug_check(Semaphore const& semaphore) {
    CTH_CRITICAL(!semaphore.created(), "semaphore must be created") {}
}

}
