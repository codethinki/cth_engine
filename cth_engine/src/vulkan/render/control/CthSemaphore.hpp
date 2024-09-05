#pragma once
#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/utility/cth_vk_types.hpp"

#include <vulkan/vulkan.h>

#include<cth/pointers.hpp>

namespace cth::vk {
class BasicCore;


class Semaphore {
public:
    struct State;

    /**
     * @brief base constructor
     */
    explicit Semaphore(cth::not_null<BasicCore const*> core);

    /**
     * @brief constructs and wraps
     * @note calls @ref wrap()
     * @note calls @ref Semaphore::Semaphore(not_null<BasicCore const*>)
     */
    Semaphore(cth::not_null<BasicCore const*> core, State const& state);

    /**
     * @brief constructs and creates
     * @note might call @ref create()
     * @note calls @ref Semaphore::Semaphore(not_null<BasicCore const*>)
     */
    explicit Semaphore(cth::not_null<BasicCore const*> core, bool create);

    virtual ~Semaphore() { optDestroy(); }

    /**
     * @brief wraps the @param state
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
     * @note submits to BasicCore::destructionQueue() if available
     * @note calls @ref destroy(VkDevice, VkSemaphore)
     */
    void destroy();
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases ownership and resets
     * @attention requires @ref created()
     */
    // ReSharper disable once CppHiddenFunction
    State release();


    static void destroy(VkDevice vk_device, VkSemaphore vk_semaphore);

protected:
    virtual VkSemaphoreCreateInfo createInfo();
    virtual void createHandle(VkSemaphoreCreateInfo const& info);
    virtual void reset();

    cth::not_null<BasicCore const*> _core;

private:
    move_ptr<VkSemaphore_T> _handle{};

public:
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkSemaphore get() const { return _handle.get(); }

    Semaphore(Semaphore const& other) = default;
    Semaphore(Semaphore&& other) = default;
    Semaphore& operator=(Semaphore const& other) = default;
    Semaphore& operator=(Semaphore&& other) = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(cth::not_null<Semaphore const*> semaphore);

#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) Semaphore::debug_check(semaphore_ptr)
#else
#define DEBUG_CHECK_SEMAPHORE(semaphore_ptr) ((void)0)
#endif

};

} //namespace cth

namespace cth::vk {
struct Semaphore::State {
    vk::not_null<VkSemaphore> vkSemaphore;
};
}
