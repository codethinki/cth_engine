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

#include <array>
#include <limits>
#include <utility>


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
     * @brief acquires an image, clears it and presents it
     * @return result of @ref Swapchain::present()
     */
    VkResult present_cleared_image(test::PresentContext const& context, Swapchain& swapchain) {
        Semaphore const acquired{context.core, create};
        auto const [result, index] = swapchain.acquire(acquired);
        EXPECT_TRUE(index.has_value()) << "acquire result: " << result;
        if(!index.has_value()) return result;

        auto const& image = swapchain.images()[*index];
        CmdPool const pool{context.core, CmdPool::Config::Default(context.queue()), create};
        PrimaryCmdBuffer cmd{pool, create};

        cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Barrier toAttachment{};
        toAttachment.addTransition(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
        toAttachment.execute(cmd);

        cmd.beginRendering(
            {
                .colorAttachments = {
                    RenderingAttachment{
                        .view = &swapchain.views()[*index],
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

        Fence const fence{context.core, 0};
        context.queue().submit(
            {
                .cmdBuffers = {&cmd},
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
     * @brief waits for the acquire semaphore of an image that was not presented
     */
    void consume(test::PresentContext const& context, Semaphore const& acquired) {
        Fence const fence{context.core, 0};
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
    EXPECT_EQ(swapchain.views().size(), swapchain.imageCount());
    EXPECT_EQ(swapchain.extent().width, Swapchain::evalExtent(EXTENT, capabilities).width);
    EXPECT_EQ(swapchain.format(), context.surface().format(context.core.physicalDevice()).format);
    EXPECT_EQ(swapchain.images()[0].aspectMask(), VK_IMAGE_ASPECT_COLOR_BIT);

    swapchain.destroy();
    EXPECT_FALSE(swapchain.created());
    EXPECT_EQ(swapchain.imageCount(), 0);
}

CVK_TEST(Swapchain, acquire_clear_present) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};

    for(uint32_t frame = 0; frame < swapchain.imageCount() + 1; ++frame) {
        auto const result = present_cleared_image(context, swapchain);
        EXPECT_TRUE(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) << "present result: " << result;
    }
}

CVK_TEST(Swapchain, resize_retires_the_old_swapchain) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);

    swapchain.resize({32, 32});

    EXPECT_EQ(swapchain.retired(), 1);
    EXPECT_EQ(swapchain.extent().width, 32);
    EXPECT_EQ(present_cleared_image(context, swapchain), VK_SUCCESS);
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

CVK_TEST(Swapchain, resize_unacquires_the_acquired_images) {
    test::PresentContext const context{};
    Swapchain swapchain{context.core, context.queue(), context.surface(), {}, EXTENT};
    Semaphore const acquired{context.core, create};

    auto const [result, index] = swapchain.acquire(acquired);
    ASSERT_TRUE(index.has_value());

    swapchain.resize({32, 32});

    consume(context, acquired);
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
}
