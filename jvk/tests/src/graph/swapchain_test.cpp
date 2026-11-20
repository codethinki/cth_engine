#include "jvk/graph/swapchain.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/utility/vk_exceptions.hpp"

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
        } catch(cvk::cvk_exception const&) { return false; }
        return true;
    }

    /**
     * acquires an image of the current swapchain
     * @return the image index, the semaphore of the acquire is waited for
     */
    [[nodiscard]] uint32_t acquire(Core const& core, Swapchain& swapchain) {
        cvk::Semaphore const acquired{core, cvk::create};
        auto const [result, index] = swapchain.acquire(acquired);
        EXPECT_TRUE(index.has_value()) << "acquire result: " << result;

        cvk::Fence const fence{core, cvk::create};
        core.queue(0).vk().submit({.waits = {acquired}}, &fence);
        fence.wait();
        return index.value_or(0);
    }
}

JVK_TEST(Swapchain, recreate_retires_the_current_swapchain) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Swapchain swapchain{core, core.queue(0).vk(), surface};
    EXPECT_FALSE(swapchain.created());
    swapchain.create({}, EXTENT);
    auto const* old = &swapchain.current();

    swapchain.recreate({32, 32});

    EXPECT_EQ(swapchain.retired(), 1);
    EXPECT_NE(&swapchain.current(), old);
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

    Swapchain swapchain{core, core.queue(0).vk(), surface};
    swapchain.create({}, EXTENT);
    auto& old = swapchain.current();
    auto const oldIdx = acquire(core, swapchain);

    // the retired swapchain holds an acquired image: the acquire keeps it
    swapchain.recreate({32, 32});
    auto const currentIdx = acquire(core, swapchain);
    EXPECT_EQ(swapchain.retired(), 1);

    old.unacquire(std::array{oldIdx});
    swapchain.current().unacquire(std::array{currentIdx});
    static_cast<void>(acquire(core, swapchain));
    EXPECT_EQ(swapchain.retired(), 0);

    swapchain.destroy();
    EXPECT_FALSE(swapchain.created());
}
}
