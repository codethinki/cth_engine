#include "jvk/graph/swapchain.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"

#include "cvk/utility/vk_exceptions.hpp"

#include <volk.h>

#include <cth/ptr/ref_wrapper.hpp>

#include <array>
#include <optional>
#include <utility>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    constexpr VkExtent2D EXTENT{64, 64};

    /**
     * @param[out] coreOpt gets a core with a queue that presents to the surface
     * @return `false` -> no device presents to a headless surface
     */
    [[nodiscard]] bool create_present_core(
        std::optional<Core>& coreOpt,
        test::Context const& context,
        cvk::Surface const& surface
    ) {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = {GRAPHICS | PRESENT};
        try {
            coreOpt.emplace(
                context.instance,
                std::move(config),
                std::array{cth::ref_wrapper{surface}}
            );
        }
        catch(cvk::cvk_exception const&) { return false; }
        return true;
    }

    /** the functions of the device table before the hooks */
    PFN_vkGetFenceStatus realGetFenceStatus = nullptr;
    PFN_vkCreateSwapchainKHR realCreateSwapchain = nullptr;

    VKAPI_ATTR VkResult VKAPI_CALL unsignaled_fence_status(VkDevice, VkFence) {
        return VK_NOT_READY;
    }

    VKAPI_ATTR VkResult VKAPI_CALL failing_create_swapchain(
        VkDevice,
        VkSwapchainCreateInfoKHR const*,
        VkAllocationCallbacks const*,
        VkSwapchainKHR*
    ) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    /**
     * the device table of the core, it reads the table at each call
     */
    [[nodiscard]] VolkDeviceTable& device_table(Core const& core) {
        return *const_cast<VolkDeviceTable*>(static_cast<cvk::Core const&>(core).functions());
    }

    /**
     * acquires an image of the current presenter, without a graph
     */
    [[nodiscard]] std::optional<Presenter::AcquiredImage> acquire(
        Core const& core,
        Swapchain& swapchain
    ) {
        return core.executor().sync_wait(swapchain.acquire(nullptr));
    }
}

JVK_TEST(Swapchain, recreate_retires_the_current_swapchain) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Swapchain swapchain{core, core.queue(0), surface, 2};
    EXPECT_FALSE(swapchain.created());
    swapchain.create({}, EXTENT);
    auto const* old = &swapchain.current();
    auto const* oldPresenter = &swapchain.presenter();
    EXPECT_EQ(&oldPresenter->swapchain(), old);
    EXPECT_EQ(oldPresenter->framesInFlight(), 2);

    swapchain.recreate({32, 32});

    EXPECT_EQ(swapchain.retired(), 1);
    EXPECT_NE(&swapchain.current(), old);
    EXPECT_EQ(&swapchain.presenter().swapchain(), &swapchain.current());
    EXPECT_EQ(swapchain.current().extent().width, 32);
    // the address of the retired swapchain stays
    EXPECT_TRUE(old->created());
}

JVK_TEST(Swapchain, acquire_destroys_the_finished_retired_swapchains) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Swapchain swapchain{core, core.queue(0), surface, 2};
    swapchain.create({}, EXTENT);

    // a spare of the retired presenter: it finished, its destruction releases the spare
    auto oldImageOpt = acquire(core, swapchain);
    ASSERT_TRUE(oldImageOpt.has_value());
    swapchain.presenter().drop(std::move(*oldImageOpt));
    swapchain.recreate({32, 32});
    EXPECT_EQ(swapchain.retired(), 1);

    // the present fences count as unsignaled: the retired presenter did not finish
    {
        auto& table = device_table(core);
        realGetFenceStatus = std::exchange(table.vkGetFenceStatus, unsignaled_fence_status);
        auto imageOpt = acquire(core, swapchain);
        table.vkGetFenceStatus = realGetFenceStatus;
        ASSERT_TRUE(imageOpt.has_value());
        swapchain.presenter().drop(std::move(*imageOpt));
    }
    EXPECT_EQ(swapchain.retired(), 1);

    auto imageOpt = acquire(core, swapchain);
    ASSERT_TRUE(imageOpt.has_value());
    EXPECT_EQ(swapchain.retired(), 0);
    swapchain.presenter().drop(std::move(*imageOpt));

    swapchain.destroy();
    EXPECT_FALSE(swapchain.created());
}

JVK_TEST(Swapchain, failed_recreate_gives_no_image_until_the_next_recreate) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Swapchain swapchain{core, core.queue(0), surface, 2};
    swapchain.create({}, EXTENT);

    // the old swapchain counts as retired: no acquire on it
    {
        auto& table = device_table(core);
        realCreateSwapchain = std::exchange(table.vkCreateSwapchainKHR, failing_create_swapchain);
        try {
            swapchain.recreate({32, 32});
            ADD_FAILURE() << "the recreate must throw";
        }
        catch(cvk::vk_result_exception const& e) {
            EXPECT_EQ(e.vkResult(), VK_ERROR_OUT_OF_HOST_MEMORY);
        }
        table.vkCreateSwapchainKHR = realCreateSwapchain;
    }
    EXPECT_EQ(swapchain.retired(), 0);
    EXPECT_FALSE(acquire(core, swapchain).has_value());

    swapchain.recreate({32, 32});
    auto imageOpt = acquire(core, swapchain);
    ASSERT_TRUE(imageOpt.has_value());
    swapchain.presenter().drop(std::move(*imageOpt));
}
}
