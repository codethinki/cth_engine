#include "jvk/graph/presenter.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/graph/stage_graph.hpp"

#include "cvk/render/sync/barrier.hpp"
#include "cvk/utility/vk_exceptions.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    constexpr VkPipelineStageFlags2 ATTACHMENT_OUTPUT = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    /**
     * creates a core with the queues that present to the surface
     * @param[out] coreOpt gets the core
     * @return `false` -> no device presents to a headless surface
     */
    [[nodiscard]] bool create_present_core(
        std::optional<Core>& coreOpt,
        test::Context const& context,
        cvk::Surface const& surface,
        std::vector<cvk::QueueFamilyProperties> queueProperties = {GRAPHICS | PRESENT}
    ) {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = std::move(queueProperties);
        try {
            coreOpt.emplace(context.instance, std::move(config), std::array{cth::ref_wrapper{surface}});
        }
        catch(cvk::cvk_exception const&) {
            return false;
        }
        return true;
    }

    struct test_exception {};

    Frame& next_frame(Core const& core, StageGraph& graph) { return core.executor().sync_wait(graph.nextFrame()); }

    [[nodiscard]] Presenter::AcquiredImage acquire(Core const& core, Presenter& presenter, Stage& stage) {
        return core.executor().sync_wait(presenter.acquire(stage, ATTACHMENT_OUTPUT));
    }

    cth::co::executor_task<void> await_stage(Stage const& stage) { co_await stage; }

    void wait(Core const& core, Stage const& stage) { core.executor().sync_wait(await_stage(stage)); }

    /**
     * enters the strand of the queue: the jobs posted to it before ended
     */
    cth::co::executor_task<void> enter_strand(Queue const& queue) { co_await queue.executor().schedule(); }

    /**
     * @return number of the acquired images of the swapchain
     */
    [[nodiscard]] size_t acquired_images(Presenter const& presenter) {
        size_t acquired = 0;
        for(uint32_t imageIdx = 0; imageIdx < presenter.swapchain().imageCount(); ++imageIdx)
            if(presenter.swapchain().acquired(imageIdx)) ++acquired;
        return acquired;
    }

    /**
     * rendering that clears the image
     */
    [[nodiscard]] cvk::RenderingInfo clear_info(SwapchainImage const& image) {
        return cvk::RenderingInfo{
            .colorAttachments = {{
                .view = image.view,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .clearValue = {.color = {{0.f, 0.5f, 1.f, 1.f}}},
            }},
        };
    }

    /**
     * records a clear of the image, the scope ends the rendering
     */
    void clear(PrimaryCmdBuffer& primary, SwapchainImage const& image) {
        auto const rendering = primary.rendering(clear_info(image));
    }

    /**
     * acquires, clears and presents the image of the stage in the next frame
     * @details only the scopes: the primary scope submits, then the image scope presents
     * @details the acquire comes before `Stage::beginPrimary()`
     * @return the image
     */
    SwapchainImage present_frame(Core const& core, StageGraph& graph, Presenter& presenter) {
        auto& stage = next_frame(core, graph).stage(0);
        auto const image = acquire(core, presenter, stage);
        EXPECT_TRUE(image);
        auto const primary = stage.beginPrimary();
        clear(*primary, *image);
        return *image;
    }
}

JVK_TEST(Presenter, frame_loop_clears_and_presents) {
    constexpr size_t FRAMES = 6;
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};
    ASSERT_TRUE(presenter.created());
    ASSERT_TRUE(presenter.swapchain().created());

    for(size_t i = 0; i < FRAMES; ++i) {
        auto const image = present_frame(core, graph, presenter);
        EXPECT_EQ(image.extent, test::HEADLESS_EXTENT);
        EXPECT_LT(image.imageIdx, presenter.swapchain().imageCount());
    }

    presenter.destroy();
    EXPECT_FALSE(presenter.created());
}

JVK_TEST(Presenter, extent_0_gives_no_image_until_the_extent_is_larger) {
    test::Context const context{};
    std::atomic<VkExtent2D> windowExtent{VkExtent2D{0, 0}};
    auto surface = test::headless_surface(context.instance, [&windowExtent] { return windowExtent.load(); });
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};
    EXPECT_FALSE(presenter.swapchain().created());

    // no image, no wait: the stage submits without the image, the image scope does nothing
    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const image = acquire(core, presenter, stage);
        EXPECT_FALSE(image);
        auto const primary = stage.beginPrimary();
    }
    EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);

    windowExtent = VkExtent2D{32, 24};
    auto const image = present_frame(core, graph, presenter);
    EXPECT_EQ(image.extent, (VkExtent2D{32, 24}));
    EXPECT_TRUE(presenter.swapchain().created());
}

JVK_TEST(Presenter, new_extent_recreates_the_swapchain) {
    test::Context const context{};
    std::atomic<VkExtent2D> windowExtent{VkExtent2D{32, 32}};
    auto surface = test::headless_surface(context.instance, [&windowExtent] { return windowExtent.load(); });
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    EXPECT_EQ(present_frame(core, graph, presenter).extent, (VkExtent2D{32, 32}));

    windowExtent = VkExtent2D{48, 40};
    EXPECT_EQ(present_frame(core, graph, presenter).extent, (VkExtent2D{48, 40}));
    EXPECT_EQ(presenter.swapchain().extent(), (VkExtent2D{48, 40}));

    // the next frames present on the new swapchain
    EXPECT_EQ(present_frame(core, graph, presenter).extent, (VkExtent2D{48, 40}));
}

JVK_TEST(Presenter, unacquire_gives_the_image_back_early) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto image = acquire(core, presenter, stage);
        ASSERT_TRUE(image);
        auto const imageIdx = image->imageIdx;
        EXPECT_TRUE(presenter.swapchain().acquired(imageIdx));

        core.executor().sync_wait(image.unacquire());
        EXPECT_FALSE(presenter.swapchain().acquired(imageIdx));

        // the submit waits for the acquire semaphore
        auto const primary = stage.beginPrimary();
    }
    // the image scope did nothing at its end
    wait(core, stage);
    EXPECT_EQ(acquired_images(presenter), 0);

    present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, image_scope_unacquires_the_image_of_a_skipped_stage) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    // beginPrimary() before the acquire works too
    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const primary = stage.beginPrimary();
        auto const image = acquire(core, presenter, stage);
        ASSERT_TRUE(image);
        clear(*primary, *image);

        // the skip discards the recording and consumes the acquire semaphore
        stage.skip();
    }
    wait(core, stage);
    core.executor().sync_wait(enter_strand(core.queue(0)));
    EXPECT_EQ(acquired_images(presenter), 0);

    present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, image_scope_ending_before_the_submit_unacquires_the_image) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const image = acquire(core, presenter, stage);
        ASSERT_TRUE(image);
    }
    core.executor().sync_wait(enter_strand(core.queue(0)));
    EXPECT_EQ(acquired_images(presenter), 0);

    // the submit waits for the acquire semaphore, nothing uses the image
    {
        auto const primary = stage.beginPrimary();
    }
    wait(core, stage);

    present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, present_inside_the_rendering_scope_submits_and_presents_once) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto image = acquire(core, presenter, stage);
        ASSERT_TRUE(image);
        auto const primary = stage.beginPrimary();
        auto const rendering = primary->rendering(clear_info(*image));

        // the submit ends the rendering and the primary, the ends of the scopes do nothing
        image.present();
        EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);
        EXPECT_EQ(primary->rendering(), nullptr);
    }
    wait(core, stage);

    present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, moved_scopes_act_once) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    auto& stage = next_frame(core, graph).stage(0);
    {
        // the moved-from scopes end after the moved ones: a second submit or present fails
        auto image = acquire(core, presenter, stage);
        auto const movedImage = std::move(image);
        ASSERT_TRUE(movedImage);
        auto primary = stage.beginPrimary();
        auto const movedPrimary = std::move(primary);
        clear(*movedPrimary, *movedImage);
    }
    EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);
    wait(core, stage);

    present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, exception_in_the_recording_submits_and_presents_nothing) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 1}};
    Presenter presenter{core, core.queue(0), {}, std::move(surface)};

    Stage const* stagePtr = nullptr;
    uint32_t imageIdx = 0;
    try {
        for(size_t i = 0;; ++i) {
            auto& stage = next_frame(core, graph).stage(0);
            auto const image = acquire(core, presenter, stage);
            auto const primary = stage.beginPrimary();
            auto const rendering = primary->rendering(clear_info(*image));
            if(i == 2) {
                stagePtr = &stage;
                imageIdx = image->imageIdx;
                throw test_exception{};
            }
        }
    }
    catch(test_exception const&) {}

    ASSERT_NE(stagePtr, nullptr);
    EXPECT_EQ(stagePtr->status(), Stage::Status::UNSUBMITTED);
    EXPECT_TRUE(presenter.swapchain().acquired(imageIdx));
    auto const& semaphore = stagePtr->semaphore();
    auto const frameNumber = stagePtr->frameNumber();

    // the next frame of the slot skips the stage, the next acquire gives the image back first
    auto& stage = next_frame(core, graph).stage(0);
    EXPECT_GE(semaphore.submittedValue(), frameNumber);
    {
        auto const image = acquire(core, presenter, stage);
        ASSERT_TRUE(image);
        EXPECT_EQ(acquired_images(presenter), 1);
        auto const primary = stage.beginPrimary();
        clear(*primary, *image);
    }

    present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, exception_propagated_to_the_end_destroys_graph_and_presenter) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface)) GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    auto const run = [&core, &surface] {
        StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
        Presenter presenter{core, core.queue(0), {}, std::move(surface)};
        for(size_t i = 0;; ++i) {
            auto& stage = next_frame(core, graph).stage(0);
            auto const image = acquire(core, presenter, stage);
            auto const primary = stage.beginPrimary();
            auto const rendering = primary->rendering(clear_info(*image));
            if(i == 2) throw test_exception{};
        }
    };
    EXPECT_THROW(run(), test_exception);
}

JVK_TEST(Presenter, present_queue_of_the_same_family) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface, {GRAPHICS, GRAPHICS | PRESENT}))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    if(&core.queue(0) == &core.queue(1) || core.queue(0).vk().familyIndex() != core.queue(1).vk().familyIndex())
        GTEST_SKIP() << "needs two vulkan queues of one family, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(1), {}, std::move(surface)};

    for(size_t i = 0; i < FRAMES; ++i)
        present_frame(core, graph, presenter);
}

JVK_TEST(Presenter, present_queue_of_another_family) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface, {GRAPHICS, COMPUTE | PRESENT}))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto const& stageQueue = core.queue(0);
    if(stageQueue.vk().familyIndex() == core.queue(1).vk().familyIndex())
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Presenter presenter{core, core.queue(1), {}, std::move(surface)};

    for(size_t i = 0; i < FRAMES; ++i)
        present_frame(core, graph, presenter);
    presenter.destroy();

    // one release on the stage queue per frame: the next release gets the next value of its counter
    auto const next = core.executor().sync_wait(stageQueue.release(cvk::Barrier{}));
    EXPECT_EQ(next.value, FRAMES + 1);
    stageQueue.waitIdle();
}
}
