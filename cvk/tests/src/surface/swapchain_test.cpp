#include "context.hpp"
#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/queue/queue.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/surface/swapchain.hpp"
#include "cvk/utility/constant/os_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/os/wait.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <thread>
#include <utility>
#include <vector>


namespace cvk {
namespace {
    constexpr VkExtent2D EXTENT{64, 64};
    constexpr uint32_t ANY = std::numeric_limits<uint32_t>::max();

    constexpr VkSurfaceCapabilitiesKHR capabilities(
        VkExtent2D currentExtent,
        uint32_t minImageCount = 2,
        uint32_t maxImageCount = 0
    ) {
        return VkSurfaceCapabilitiesKHR{
            .minImageCount = minImageCount,
            .maxImageCount = maxImageCount,
            .currentExtent = currentExtent,
            .minImageExtent = {16, 16},
            .maxImageExtent = {1024, 1024},
            .maxImageArrayLayers = 1,
            .supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        };
    }

    /**
     * acquires an image, clears it and presents it
     * @return result of `Swapchain::present()`
     */
    VkResult present_cleared_image(test::PresentContext const& context, Swapchain& swapchain) {
        Semaphore const acquired{context.core, create};
        auto const [result, index] = swapchain.acquire(acquired);
        EXPECT_TRUE(index.has_value()) << "acquire result: " << result;
        if(!index.has_value()) return result;

        auto const& image = swapchain.image(*index);
        CmdPool const pool{context.core, test::pool_config(context.core), create};
        PrimaryCmdBuffer cmd{pool, create};

        cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Barrier toAttachment{};
        toAttachment.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toAttachment.execute(cmd);

        cmd.beginRendering(
            {
                .colorAttachments = {
                    RenderingAttachment{
                        .view = swapchain.view(*index),
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .clearValue = {.color = {.float32 = {0.f, 0.f, 1.f, 1.f}}},
                    },
                },
            }
        );
        cmd.endRendering();

        Barrier toPresent{};
        toPresent.addTransition(image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        toPresent.execute(cmd);

        cmd.end();

        Fence const fence{context.core, create};
        context.queue().submit(
            {
                .cmdBuffers = {cmd},
                .waits = {{acquired, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT}},
                .signals = {swapchain.presentSemaphore(*index)},
            },
            &fence
        );
        auto const presented = swapchain.present(*index);
        fence.wait();

        return presented;
    }

    /**
     * waits for the acquire semaphore of an image that was not presented
     */
    void consume(test::PresentContext const& context, Semaphore const& acquired) {
        Fence const fence{context.core, create};
        context.queue().submit({.waits = {acquired}}, &fence);
        fence.wait();
    }
}

CVK_TEST(Swapchain, eval_extent) {
    static_assert(Swapchain::evalExtent(EXTENT, capabilities({800, 600})).width == 800);
    static_assert(Swapchain::evalExtent(EXTENT, capabilities({ANY, ANY})).width == EXTENT.width);
    static_assert(Swapchain::evalExtent({4096, 8}, capabilities({ANY, ANY})).width == 1024);
    static_assert(Swapchain::evalExtent({4096, 8}, capabilities({ANY, ANY})).height == 16);
}

CVK_TEST(Swapchain, eval_image_count) {
    static_assert(Swapchain::evalImageCount(0, capabilities({ANY, ANY}, 2, 0)) == 3);
    static_assert(Swapchain::evalImageCount(0, capabilities({ANY, ANY}, 2, 2)) == 2);
    static_assert(Swapchain::evalImageCount(1, capabilities({ANY, ANY}, 2, 8)) == 2);
    static_assert(Swapchain::evalImageCount(5, capabilities({ANY, ANY}, 2, 0)) == 5);
}

CVK_TEST(Swapchain, base_constructor_does_not_create) {
    test::PresentContext const context{};

    Swapchain const swapchain{context.core, context.queue(), context.surface(), {}};

    EXPECT_FALSE(swapchain.created());
    EXPECT_EQ(swapchain.imageCount(), 0);
}

CVK_TEST(Swapchain, create_and_destroy) {
    test::PresentContext const context{};
    auto const capabilities = context.surface().capabilities(context.core.physicalDevice());

    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};

    ASSERT_TRUE(swapchain.created());
    EXPECT_GE(swapchain.imageCount(), capabilities.minImageCount);
    EXPECT_EQ(swapchain.extent().width, Swapchain::evalExtent(EXTENT, capabilities).width);
    auto const offered = context.surface().formats(context.core.physicalDevice());
    EXPECT_TRUE(std::ranges::contains(offered, VkSurfaceFormatKHR{swapchain.format(), swapchain.colorSpace()}));
    EXPECT_EQ(swapchain.image(0).aspectMask(), VK_IMAGE_ASPECT_COLOR_BIT);

    swapchain.destroy();
    EXPECT_FALSE(swapchain.created());
    EXPECT_EQ(swapchain.imageCount(), 0);
}

CVK_TEST(Swapchain, create_picks_the_first_allowed_format_the_surface_offers) {
    test::PresentContext const context{};
    auto const offered = context.surface().formats(context.core.physicalDevice());
    if(offered.size() < 2) GTEST_SKIP() << "needs two surface formats";

    Swapchain::Config const config{
        .formats = {{VK_FORMAT_R64G64B64A64_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, offered[1], offered[0]},
    };
    Swapchain const swapchain{context.core, context.queue(), context.surface(), config, EXTENT};

    EXPECT_EQ(swapchain.format(), offered[1].format);
    EXPECT_EQ(swapchain.colorSpace(), offered[1].colorSpace);
}

CVK_TEST(Swapchain, create_without_an_offered_format_throws) {
    test::PresentContext const context{};
    Swapchain::Config const config{.formats = {{VK_FORMAT_R64G64B64A64_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};
    Swapchain swapchain{context.core, context.queue(), context.surface(), config};

    EXPECT_THROW(swapchain.create(EXTENT), cvk_exception);
    EXPECT_FALSE(swapchain.created());
}

CVK_TEST(Swapchain, create_without_an_offered_present_mode_throws) {
    test::PresentContext const context{};
    Swapchain::Config const config{.presentModes = {VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR}};
    Swapchain swapchain{context.core, context.queue(), context.surface(), config};

    EXPECT_THROW(swapchain.create(EXTENT), cvk_exception);
    EXPECT_FALSE(swapchain.created());
}

CVK_TEST(Swapchain, empty_lists_take_the_first_format_and_fifo) {
    test::PresentContext const context{};
    auto const offered = context.surface().formats(context.core.physicalDevice());

    Swapchain::Config const config{.formats = {}, .presentModes = {}};
    Swapchain const swapchain{context.core, context.queue(), context.surface(), config, EXTENT};

    EXPECT_EQ(swapchain.format(), offered[0].format);
    EXPECT_EQ(swapchain.colorSpace(), offered[0].colorSpace);
    EXPECT_EQ(swapchain.presentMode(), VK_PRESENT_MODE_FIFO_KHR);
}

CVK_TEST(Swapchain, acquire_clear_present) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};

    for(uint32_t frame = 0; frame < swapchain.imageCount() + 1; ++frame) {
        auto const result = present_cleared_image(context, swapchain);
        EXPECT_TRUE(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) << "present result: " << result;
    }
}

CVK_TEST(Swapchain, create_with_an_old_swapchain) {
    test::PresentContext const context{};
    Swapchain old{context.core, context.queue(), context.surface(), {}, EXTENT};
    EXPECT_EQ(present_cleared_image(context, old), VK_SUCCESS);

    Swapchain const swapchain{context.core, context.queue(), context.surface(), {}, {32, 32}, &old};

    // the old swapchain stays until its owner destroys it
    EXPECT_TRUE(old.created());
    EXPECT_EQ(swapchain.extent().width, 32);
    EXPECT_NE(swapchain.get(), old.get());
}

CVK_TEST(Swapchain, old_swapchain_presents_its_acquired_image) {
    test::PresentContext const context{};
    Swapchain old{context.core, context.queue(), context.surface(), {}, EXTENT};
    Semaphore const acquired{context.core, create};
    auto const [result, index] = old.acquire(acquired);
    ASSERT_TRUE(index.has_value());

    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, {32, 32}, &old};

    // the present of the retired swapchain: no validation error
    CmdPool const pool{context.core, test::pool_config(context.core), create};
    PrimaryCmdBuffer cmd{pool, create};
    cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    Barrier toPresent{};
    toPresent.addTransition(old.image(*index), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_UNDEFINED);
    toPresent.execute(cmd);
    cmd.end();
    Fence const fence{context.core, create};
    context.queue().submit(
        {
            .cmdBuffers = {cmd},
            .waits = {{acquired, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}},
            .signals = {old.presentSemaphore(*index)},
        },
        &fence
    );
    auto const presented = old.present(*index);
    EXPECT_TRUE(presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR || presented == VK_ERROR_OUT_OF_DATE_KHR)
        << "present result: " << presented;
    fence.wait();

    EXPECT_FALSE(old.acquired());
    old.destroy();
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);
}

CVK_TEST(Swapchain, acquired_and_presented) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    EXPECT_FALSE(swapchain.acquired());
    EXPECT_TRUE(swapchain.presented());

    Semaphore const acquired{context.core, create};
    auto const [result, index] = swapchain.acquire(acquired);
    ASSERT_TRUE(index.has_value());
    EXPECT_TRUE(swapchain.acquired());
    swapchain.unacquire(std::array{*index});
    EXPECT_FALSE(swapchain.acquired());
    consume(context, acquired);

    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);
    EXPECT_FALSE(swapchain.acquired());
    // the headless surface completes presents at once, the bound only ends a broken loop
    for(uint32_t i = 0; i < 1000 && !swapchain.presented(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    EXPECT_TRUE(swapchain.presented());
}

CVK_TEST(Swapchain, present_fences_are_exportable) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);

    for(uint32_t i = 0; i < swapchain.imageCount(); ++i) {
        auto const& fence = swapchain.presentFence(i);
        EXPECT_EQ(fence.exportTypes(), constants::OS_FENCE_HANDLE_TYPE);

        // invalid handle -> already signaled
        auto const handle = fence.extractOsHandle();
        if(handle) EXPECT_EQ(cth::os::wait(handle.get(), std::chrono::seconds{5}), cth::os::wait_result::WAITED);
        EXPECT_TRUE(fence.signaled());
    }

    // the presents and the destruction wait for the exported fences
    for(uint32_t frame = 0; frame < swapchain.imageCount() + 1; ++frame)
        EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);

    swapchain.destroy();
    EXPECT_FALSE(swapchain.created());
}

CVK_TEST(Swapchain, unacquire_gives_the_image_back) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    Semaphore const acquired{context.core, create};

    auto const [result, index] = swapchain.acquire(acquired);
    ASSERT_TRUE(index.has_value());
    EXPECT_TRUE(swapchain.acquired(*index));

    swapchain.unacquire(std::array{*index});

    EXPECT_FALSE(swapchain.acquired(*index));
    consume(context, acquired);
}

CVK_TEST(Swapchain, old_swapchain_unacquires_its_image) {
    test::PresentContext const context{};
    Swapchain old{context.core, context.queue(), context.surface(), {}, EXTENT};
    Semaphore const acquired{context.core, create};

    auto const [result, index] = old.acquire(acquired);
    ASSERT_TRUE(index.has_value());

    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, {32, 32}, &old};
    old.unacquire(std::array{*index});

    consume(context, acquired);
    EXPECT_FALSE(old.acquired());
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);
}

CVK_TEST(Swapchain, release_and_wrap) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    auto const vkSwapchain = swapchain.get();
    auto const imageCount = swapchain.imageCount();

    auto const state = swapchain.release();
    EXPECT_FALSE(swapchain.created());

    Swapchain wrapped{context.core, context.queue(), context.surface(), {}, state};
    EXPECT_EQ(wrapped.get(), vkSwapchain);
    EXPECT_EQ(wrapped.imageCount(), imageCount);
    EXPECT_EQ(present_cleared_image(context, wrapped), VK_SUCCESS);
}

CVK_TEST(Swapchain, move_assignment_destroys_the_old_swapchain) {
    test::PresentContext const context{};
    auto const other = test::headless_surface(context.instance);
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    Swapchain moved{context.core, context.queue(), other, {}, EXTENT};
    auto const vkSwapchain = moved.get();

    swapchain = std::move(moved);

    EXPECT_EQ(swapchain.get(), vkSwapchain);
    EXPECT_FALSE(moved.created());
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);
}

CVK_TEST(Swapchain, move_assignment_over_a_swapchain_with_a_present_in_flight) {
    test::PresentContext const context{};
    auto const other = test::headless_surface(context.instance);
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    // the move assignment waits for the present fence, then destroys the swapchain
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);

    Swapchain moved{context.core, context.queue(), other, {}, EXTENT};
    auto const vkSwapchain = moved.get();

    swapchain = std::move(moved);

    ASSERT_TRUE(swapchain.created());
    EXPECT_EQ(swapchain.get(), vkSwapchain);
    EXPECT_EQ(swapchain.extent().width, EXTENT.width);
    EXPECT_FALSE(moved.created());
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);
}

CVK_TEST(Swapchain, decompose_returns_the_released_handle) {
    // a leak shows up when the shared device is destroyed
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    auto const vkSwapchain = swapchain.get();

    auto const [vkReleased] = swapchain.release().decompose();
    EXPECT_EQ(vkReleased, vkSwapchain);

    cvk::destroy(context.core.deviceTable(), vkReleased);
}
}
