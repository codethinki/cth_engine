#include "jvk/graph/surface.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/data/image.hpp"
#include "jvk/graph/stage_graph.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/utility/vk_exceptions.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>
#include <cth/ptr/ref_wrapper.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>
#include <utility>
#include <vector>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    constexpr VkPipelineStageFlags2 ATTACHMENT_OUTPUT =
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    constexpr auto TIMEOUT = std::chrono::seconds{5};

    /**
     * creates a core with the queues that present to the surface
     * @param[out] coreOpt gets the core
     * @return `false` -> no device presents to a headless surface
     */
    [[nodiscard]] bool create_present_core(
        std::optional<Core>& coreOpt,
        test::Context const& context,
        cvk::Surface const& windowSurface,
        std::vector<cvk::QueueFamilyProperties> queueProperties = {GRAPHICS | PRESENT},
        size_t driverThreads = 0
    ) {
        auto config = test::Context::CONFIG;
        config.vk.queueProperties = std::move(queueProperties);
        config.driverThreads = driverThreads;
        try {
            coreOpt.emplace(
                context.instance,
                std::move(config),
                std::array{cth::ref_wrapper{windowSurface}}
            );
        }
        catch(cvk::cvk_exception const&) { return false; }
        return true;
    }

    struct test_exception {};

    Frame& next_frame(Core const& core, StageGraph& graph) {
        return core.executor().sync_wait(graph.nextFrame());
    }

    [[nodiscard]] std::optional<SwapchainImage> acquire(
        Core const& core,
        Surface& surface,
        Stage& stage
    ) {
        return core.executor().sync_wait(surface.acquire(stage, ATTACHMENT_OUTPUT));
    }

    [[nodiscard]] std::optional<SwapchainImage> acquire(
        Core const& core,
        Surface& surface,
        Stage& acquireStage,
        Stage& presentStage
    ) {
        return core.executor().sync_wait(
            surface.acquire(acquireStage, presentStage, ATTACHMENT_OUTPUT)
        );
    }

    cth::co::executor_task<void> await_stage(Stage const& stage) { co_await stage; }

    /**
     * signals the semaphore from the host, without a mark
     */
    void host_signal(Semaphore const& semaphore, uint64_t value) {
        cvk::Core const& vk = semaphore.core();
        VkSemaphoreSignalInfo const info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .pNext = nullptr,
            .semaphore = semaphore.get(),
            .value = value,
        };
        ASSERT_EQ(vk.functions()->vkSignalSemaphore(vk.vkDevice(), &info), VK_SUCCESS);
    }


    void wait(Core const& core, Stage const& stage) {
        core.executor().sync_wait(await_stage(stage));
    }

    /** the image `vkAcquireNextImageKHR()` gave, without its present or release */
    struct TrackedImage {
        VkSwapchainKHR vkSwapchain;
        uint32_t imageIdx;

        constexpr bool operator==(TrackedImage const& other) const = default;
    };

    std::mutex trackedMutex{};
    std::vector<TrackedImage> trackedImages{};

    /** the functions of the device table before the tracking hooks */
    PFN_vkAcquireNextImageKHR trackedAcquire = nullptr;
    PFN_vkQueuePresentKHR trackedPresent = nullptr;
    PFN_vkReleaseSwapchainImagesKHR trackedRelease = nullptr;

    void untrack(VkSwapchainKHR vkSwapchain, uint32_t imageIdx) {
        std::scoped_lock const lock{trackedMutex};
        std::erase(trackedImages, TrackedImage{vkSwapchain, imageIdx});
    }

    VKAPI_ATTR VkResult VKAPI_CALL tracking_acquire(
        VkDevice device,
        VkSwapchainKHR vkSwapchain,
        uint64_t timeout,
        VkSemaphore vkSemaphore,
        VkFence vkFence,
        uint32_t* imageIdx
    ) {
        auto const result =
            trackedAcquire(device, vkSwapchain, timeout, vkSemaphore, vkFence, imageIdx);
        if(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
            std::scoped_lock const lock{trackedMutex};
            trackedImages.push_back({vkSwapchain, *imageIdx});
        }
        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL tracking_present(VkQueue queue, VkPresentInfoKHR const* info) {
        auto const result = trackedPresent(queue, info);
        for(uint32_t i = 0; i < info->swapchainCount; ++i)
            untrack(info->pSwapchains[i], info->pImageIndices[i]);
        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL tracking_release(
        VkDevice device,
        VkReleaseSwapchainImagesInfoKHR const* info
    ) {
        auto const result = trackedRelease(device, info);
        if(result == VK_SUCCESS)
            for(uint32_t i = 0; i < info->imageIndexCount; ++i)
                untrack(info->swapchain, info->pImageIndices[i]);
        return result;
    }

    /**
     * tracks the acquired images of the core through hooks of its device table
     * @details
     * - an acquire adds the image, a present or a release removes it
     * - thread safe: the hooks run in the strands and on the driver threads
     * - the core reads the device table at each call
     */
    class AcquireTracker {
    public:
        explicit AcquireTracker(Core const& core) :
            _table{*const_cast<VolkDeviceTable*>(static_cast<cvk::Core const&>(core).functions())} {
            trackedImages.clear();
            trackedAcquire = std::exchange(_table->vkAcquireNextImageKHR, tracking_acquire);
            trackedPresent = std::exchange(_table->vkQueuePresentKHR, tracking_present);
            trackedRelease = std::exchange(_table->vkReleaseSwapchainImagesKHR, tracking_release);
        }

        ~AcquireTracker() {
            _table->vkAcquireNextImageKHR = trackedAcquire;
            _table->vkQueuePresentKHR = trackedPresent;
            _table->vkReleaseSwapchainImagesKHR = trackedRelease;
        }

        /**
         * @return number of the acquired images of the swapchain
         */
        [[nodiscard]] size_t count(cvk::Swapchain const& swapchain) const {
            std::scoped_lock const lock{trackedMutex};
            return static_cast<size_t>(
                std::ranges::count(trackedImages, swapchain.get(), &TrackedImage::vkSwapchain)
            );
        }

        [[nodiscard]] bool acquired(cvk::Swapchain const& swapchain, uint32_t imageIdx) const {
            std::scoped_lock const lock{trackedMutex};
            return std::ranges::contains(trackedImages, TrackedImage{swapchain.get(), imageIdx});
        }

    private:
        cth::ref_wrapper<VolkDeviceTable> _table;

    public:
        AcquireTracker(AcquireTracker const& other) = delete;
        AcquireTracker& operator=(AcquireTracker const& other) = delete;
    };

    /**
     * @return number of the acquired images of the current swapchain
     */
    [[nodiscard]] size_t acquired_images(AcquireTracker const& tracker, Surface const& surface) {
        return tracker.count(surface.swapchain().current());
    }

    /**
     * @return the image is no longer acquired before the timeout, e.g. presented
     */
    [[nodiscard]] bool reaches_unacquired(
        AcquireTracker const& tracker,
        Surface const& surface,
        uint32_t imageIdx
    ) {
        auto const deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while(tracker.acquired(surface.swapchain().current(), imageIdx)) {
            if(std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        return true;
    }

    /**
     * rendering that clears the image
     */
    [[nodiscard]] cvk::RenderingInfo clear_info(SwapchainImage const& image) {
        return cvk::RenderingInfo{
            .colorAttachments = {
                {
                    .view = image.view,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .clearValue = {.color = {{0.f, 0.5f, 1.f, 1.f}}},
                }
            },
        };
    }

    /**
     * records a clear of the image, the scope ends the rendering
     */
    void clear(PrimaryCmdBuffer& primary, SwapchainImage const& image) {
        auto const rendering = primary.beginRendering(clear_info(image));
    }

    /**
     * acquires and clears the image of the stage, the scope submits, the submission presents
     * @details the submission waits for the gate: the present stays pending
     * @return the image
     */
    SwapchainImage present_behind_gate(
        Core const& core,
        Stage& stage,
        Surface& surface,
        Semaphore const& gate
    ) {
        stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
        auto const image = acquire(core, surface, stage);
        EXPECT_TRUE(image);
        auto const primary = stage.begin();
        clear(*primary, *image);
        return *image;
    }

    constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_4_BIT;

    [[nodiscard]] bool supports_msaa(Core const& core) {
        cvk::Core const& vk = core;
        return (vk.physicalDevice().limits().framebufferColorSampleCounts & MSAA_SAMPLES) != 0;
    }

    /**
     * multisampled color image of the frames and its view, the rendering resolves it into the swapchain image
     */
    struct MsaaTarget {
        MsaaTarget(Core const& core, VkFormat format, VkExtent2D extent) :
            image{
                core,
                {
                    .format = format,
                    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                    | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                    .samples = MSAA_SAMPLES,
                },
                extent
            },
            view{image, {}, cvk::create} {}

        Image image;
        cvk::ImageView view;
    };

    /**
     * acquires the image of the next frame, clears the multisampled target and resolves it into the image
     * @details only the scope: `scoped_submit` submits, the submission presents
     * @details a new extent: the stage retains the old target, then a new one
     * @param[out] msaaOpt the target of the frames
     * @return the image
     */
    SwapchainImage present_msaa_frame(
        Core const& core,
        StageGraph& graph,
        Surface& surface,
        std::optional<MsaaTarget>& msaaOpt
    ) {
        auto& stage = next_frame(core, graph).stage(0);
        auto const image = acquire(core, surface, stage);
        EXPECT_TRUE(image);
        if(!msaaOpt.has_value() || msaaOpt->image.extent() != image->extent) {
            if(msaaOpt.has_value()) {
                stage.retain(std::move(msaaOpt->view));
                stage.retain(std::move(msaaOpt->image));
            }
            msaaOpt.emplace(core, surface.swapchain().current().format(), image->extent);
        }

        auto const primary = stage.begin();
        auto const rendering = primary->beginRendering(
            {
                .colorAttachments = {
                    {
                        .view = msaaOpt->view,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .clearValue = {.color = {{0.f, 0.5f, 1.f, 1.f}}},
                        .resolveViewPtr = &*image->view,
                        .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
                    }
                },
            }
        );
        return *image;
    }

    /**
     * acquires, clears and presents the image of the stage in the next frame
     * @details only the scope: `scoped_submit` submits, the submission presents
     * @details the acquire comes before `Stage::begin()`
     * @return the image
     */
    SwapchainImage present_frame(Core const& core, StageGraph& graph, Surface& surface) {
        auto& stage = next_frame(core, graph).stage(0);
        auto const image = acquire(core, surface, stage);
        EXPECT_TRUE(image);
        auto const primary = stage.begin();
        clear(*primary, *image);
        return *image;
    }

    /** the `vkQueuePresentKHR()` of the device table before `lost_present()` */
    PFN_vkQueuePresentKHR realQueuePresent = nullptr;

    /**
     * presents, then reports a lost surface for every swapchain
     */
    VKAPI_ATTR VkResult VKAPI_CALL lost_present(VkQueue queue, VkPresentInfoKHR const* info) {
        if(auto const result = realQueuePresent(queue, info); result < 0)
            return result;
        if(info->pResults != nullptr)
            for(uint32_t i = 0; i < info->swapchainCount; ++i)
                info->pResults[i] = VK_ERROR_SURFACE_LOST_KHR;
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    /**
     * the scene stage clears the image, the ui stage draws over it and presents
     * @details the ui stage waits for the scene stage in the frame
     */
    StageGraph::Config scene_and_ui(size_t uiQueueIdx = 0) {
        return {.stages = {{.queueIdx = 0}, {.queueIdx = uiQueueIdx, .waits = {{.stageId = 0}}}}};
    }

    constexpr size_t SCENE = 0;
    constexpr size_t UI = 1;

    /**
     * the scene stage clears the image, the ui stage loads and clears its upper half, the ui stage
     * presents
     * @return the image
     */
    SwapchainImage present_scene_and_ui(Core const& core, StageGraph& graph, Surface& surface) {
        auto& frame = next_frame(core, graph);
        auto const image = acquire(core, surface, frame[SCENE], frame[UI]);
        EXPECT_TRUE(image);
        {
            auto const scene = frame[SCENE].begin();
            clear(*scene, *image);
        }
        {
            auto const ui = frame[UI].begin();
            auto const rendering = ui->beginRendering(
                {
                    .colorAttachments = {
                        {
                            .view = image->view,
                            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                        }
                    },
                    .area = VkRect2D{
                        .offset = {0, 0},
                        .extent = {image->extent.width, image->extent.height / 2},
                    },
                }
            );
        }
        return *image;
    }
}

JVK_TEST(Surface, frame_loop_clears_and_presents) {
    constexpr size_t FRAMES = 6;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    // before the graph: the presents of the graph refer to the surface
    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    ASSERT_TRUE(surface.created());
    ASSERT_TRUE(surface.swapchain().created());

    for(size_t i = 0; i < FRAMES; ++i) {
        auto const image = present_frame(core, graph, surface);
        EXPECT_EQ(image.extent, test::HEADLESS_EXTENT);
        EXPECT_LT(image.imageIdx, surface.swapchain().current().imageCount());
    }
}

JVK_TEST(Surface, destroy_after_the_graph_of_the_stages) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    {
        StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
        present_frame(core, graph, surface);
    }
    surface.destroy();
    EXPECT_FALSE(surface.created());
}

JVK_TEST(Surface, frame_loop_with_driver_threads_presents) {
    constexpr size_t FRAMES = 6;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface, {GRAPHICS | PRESENT}, 2))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    for(size_t i = 0; i < FRAMES; ++i)
        EXPECT_EQ(present_frame(core, graph, surface).extent, test::HEADLESS_EXTENT);
}

JVK_TEST(Surface, extent_0_gives_no_image_until_the_extent_is_larger) {
    test::Context const context{};
    std::atomic<VkExtent2D> windowExtent{VkExtent2D{0, 0}};
    auto windowSurface = test::headless_surface(
        context.instance,
        [&windowExtent] { return windowExtent.load(); }
    );
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    EXPECT_FALSE(surface.swapchain().created());

    // no image, no wait, no present: the stage submits without the image
    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const image = acquire(core, surface, stage);
        EXPECT_FALSE(image.has_value());
        auto const primary = stage.begin();
    }
    EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);

    windowExtent = VkExtent2D{32, 24};
    auto const image = present_frame(core, graph, surface);
    EXPECT_EQ(image.extent, (VkExtent2D{32, 24}));
    EXPECT_TRUE(surface.swapchain().created());
}

JVK_TEST(Surface, new_extent_recreates_the_swapchain) {
    test::Context const context{};
    std::atomic<VkExtent2D> windowExtent{VkExtent2D{32, 32}};
    auto windowSurface = test::headless_surface(
        context.instance,
        [&windowExtent] { return windowExtent.load(); }
    );
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    EXPECT_EQ(present_frame(core, graph, surface).extent, (VkExtent2D{32, 32}));

    windowExtent = VkExtent2D{48, 40};
    EXPECT_EQ(present_frame(core, graph, surface).extent, (VkExtent2D{48, 40}));
    EXPECT_EQ(surface.swapchain().current().extent(), (VkExtent2D{48, 40}));

    // the next frames present on the new swapchain
    EXPECT_EQ(present_frame(core, graph, surface).extent, (VkExtent2D{48, 40}));
}

JVK_TEST(Surface, multisampled_frames_resolve_into_the_image_and_present) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    if(!supports_msaa(core))
        GTEST_SKIP() << "no 4x msaa for color attachments";

    // before the graph: the target lives until the gpu finished the frames
    std::optional<MsaaTarget> msaaOpt{};
    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    // the resolve writes the image, the present transition starts from the resolve state
    for(size_t i = 0; i < FRAMES; ++i)
        EXPECT_EQ(
        present_msaa_frame(core, graph, surface, msaaOpt).extent,
        test::HEADLESS_EXTENT
    );
}

JVK_TEST(Surface, recreation_retains_the_multisampled_target_of_the_old_extent) {
    test::Context const context{};
    std::atomic<VkExtent2D> windowExtent{VkExtent2D{32, 32}};
    auto windowSurface = test::headless_surface(
        context.instance,
        [&windowExtent] { return windowExtent.load(); }
    );
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    if(!supports_msaa(core))
        GTEST_SKIP() << "no 4x msaa for color attachments";

    std::optional<MsaaTarget> msaaOpt{};
    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    EXPECT_EQ(present_msaa_frame(core, graph, surface, msaaOpt).extent, (VkExtent2D{32, 32}));

    // the new extent recreates the swapchain, the frame retains the old target
    windowExtent = VkExtent2D{48, 40};
    EXPECT_EQ(present_msaa_frame(core, graph, surface, msaaOpt).extent, (VkExtent2D{48, 40}));
    EXPECT_EQ(msaaOpt->image.extent(), (VkExtent2D{48, 40}));

    // the graph destroys the old target after the gpu finished its frame
    for(size_t i = 0; i < 3; ++i)
        EXPECT_EQ(present_msaa_frame(core, graph, surface, msaaOpt).extent, (VkExtent2D{48, 40}));
}

JVK_TEST(Surface, acquire_does_not_wait_for_the_present_before) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const gate{core, cvk::create};

    auto& first = next_frame(core, graph).stage(0);
    auto const firstImage = present_behind_gate(core, first, surface, gate);

    // the submission of the first frame waits for the mark of the gate: its present is pending,
    // the next acquire gives another image at once
    auto& second = next_frame(core, graph).stage(0);
    std::optional<SwapchainImage> secondImageOpt{};
    std::binary_semaphore acquiredSignal{0};
    std::jthread frame{
        [&] {
            secondImageOpt = acquire(core, surface, second);
            acquiredSignal.release();
        }
    };
    EXPECT_TRUE(acquiredSignal.try_acquire_for(TIMEOUT));
    EXPECT_EQ(acquired_images(tracker, surface), 2);

    // the signal before the mark: the present waits for no signal that is not submitted
    host_signal(gate, 1);
    gate.markSubmitted(1);
    frame.join();
    ASSERT_TRUE(secondImageOpt.has_value());
    EXPECT_NE(secondImageOpt->imageIdx, firstImage.imageIdx);
    {
        auto const primary = second.begin();
        clear(*primary, *secondImageOpt);
    }

    present_frame(core, graph, surface);
}

JVK_TEST(Surface, recreation_retires_the_old_swapchain_until_its_presenter_finished) {
    test::Context const context{};
    std::atomic<VkExtent2D> windowExtent{VkExtent2D{32, 32}};
    auto windowSurface = test::headless_surface(
        context.instance,
        [&windowExtent] { return windowExtent.load(); }
    );
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    EXPECT_EQ(present_frame(core, graph, surface).extent, (VkExtent2D{32, 32}));

    // the new extent recreates the swapchain, the old one stays until its presenter finished
    windowExtent = VkExtent2D{48, 40};
    EXPECT_EQ(present_frame(core, graph, surface).extent, (VkExtent2D{48, 40}));
    EXPECT_LE(surface.swapchain().retired(), 1);

    // a later acquire destroys the retired swapchain once its presenter finished, the bound only
    // ends a broken loop
    for(size_t i = 0; i < 16 && surface.swapchain().retired() != 0; ++i)
        present_frame(core, graph, surface);
    EXPECT_EQ(surface.swapchain().retired(), 0);
}

JVK_TEST(Surface, skipped_stage_drops_the_image_for_the_next_acquire) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    // one frame slot: the next frame waits for the submission of the skip, its drop included
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 1}};

    // Stage::begin() before the acquire works too
    auto& stage = next_frame(core, graph).stage(0);
    uint32_t imageIdx = 0;
    {
        auto const primary = stage.begin();
        auto const image = acquire(core, surface, stage);
        ASSERT_TRUE(image.has_value());
        imageIdx = image->imageIdx;
        clear(*primary, *image);

        // the skip discards the recording, waits for the acquire semaphore, drops the image
        stage.skip();
    }
    EXPECT_EQ(stage.status(), Stage::Status::SKIPPED);
    EXPECT_EQ(acquired_images(tracker, surface), 1);

    // the dropped image comes first, it waits for the skip on the gpu: no validation error
    auto& next = next_frame(core, graph).stage(0);
    {
        auto const image = acquire(core, surface, next);
        ASSERT_TRUE(image.has_value());
        EXPECT_EQ(image->imageIdx, imageIdx);
        EXPECT_EQ(acquired_images(tracker, surface), 1);
        auto const primary = next.begin();
        clear(*primary, *image);
    }

    present_frame(core, graph, surface);
}

JVK_TEST(Surface, image_without_a_use_presents_after_the_submit) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    // the epilogue transitions the image from VK_IMAGE_LAYOUT_UNDEFINED
    auto& stage = next_frame(core, graph).stage(0);
    uint32_t imageIdx = 0;
    {
        auto const image = acquire(core, surface, stage);
        ASSERT_TRUE(image.has_value());
        imageIdx = image->imageIdx;
        auto const primary = stage.begin();
    }
    EXPECT_TRUE(reaches_unacquired(tracker, surface, imageIdx));
    wait(core, stage);

    present_frame(core, graph, surface);
}

JVK_TEST(Surface, exception_in_the_recording_submits_and_presents_nothing) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    // one frame slot: the next frame waits for the submission of the skip, its drop included
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 1}};

    Stage const* stagePtr = nullptr;
    uint32_t imageIdx = 0;
    try {
        for(size_t i = 0;; ++i) {
            auto& stage = next_frame(core, graph).stage(0);
            auto const image = acquire(core, surface, stage);
            auto const primary = stage.begin();
            auto const rendering = primary->beginRendering(clear_info(*image));
            if(i == 2) {
                stagePtr = &stage;
                imageIdx = image->imageIdx;
                throw test_exception{};
            }
        }
    }
    catch(test_exception const&) {}

    // the scope skipped the stage, the skip drops the image, the next acquire gives it back first
    ASSERT_NE(stagePtr, nullptr);
    EXPECT_EQ(stagePtr->status(), Stage::Status::SKIPPED);
    EXPECT_TRUE(tracker.acquired(surface.swapchain().current(), imageIdx));

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const image = acquire(core, surface, stage);
        ASSERT_TRUE(image.has_value());
        EXPECT_EQ(image->imageIdx, imageIdx);
        EXPECT_EQ(acquired_images(tracker, surface), 1);
        auto const primary = stage.begin();
        clear(*primary, *image);
    }

    present_frame(core, graph, surface);
}

JVK_TEST(Surface, exception_after_the_submit_presents_the_image) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    auto& stage = next_frame(core, graph).stage(0);
    std::optional<uint32_t> imageIdxOpt = std::nullopt;
    try {
        auto const image = acquire(core, surface, stage);
        auto const primary = stage.begin();
        if(image) {
            imageIdxOpt = image->imageIdx;
            clear(*primary, *image);
        }
        stage.submit();
        throw test_exception{};
    }
    catch(test_exception const&) {}
    ASSERT_TRUE(imageIdxOpt.has_value());
    EXPECT_EQ(stage.status(), Stage::Status::SUBMITTED);

    // the stage is submitted: its submission presents the image
    EXPECT_TRUE(reaches_unacquired(tracker, surface, *imageIdxOpt));
    wait(core, stage);

    present_frame(core, graph, surface);
}

JVK_TEST(Surface, exception_propagated_to_the_end_destroys_graph_and_surface) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    auto const run = [&core, &windowSurface] {
        Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
        StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
        for(size_t i = 0;; ++i) {
            auto& stage = next_frame(core, graph).stage(0);
            auto const image = acquire(core, surface, stage);
            auto const primary = stage.begin();
            auto const rendering = primary->beginRendering(clear_info(*image));
            if(i == 2)
                throw test_exception{};
        }
    };
    EXPECT_THROW(run(), test_exception);
}

JVK_TEST(Surface, failed_present_arrives_at_the_next_acquire_and_not_at_next_frame) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    // one frame slot: the next frame waits for the present of the frame before
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}, .frameSlots = 1}};

    // the core reads the device table at each call: the presents report a lost surface until the
    // restore
    cvk::Core const& vkCore = core;
    auto* const table = const_cast<VolkDeviceTable*>(vkCore.functions());
    realQueuePresent = std::exchange(table->vkQueuePresentKHR, lost_present);
    present_frame(core, graph, surface);

    auto& stage = next_frame(core, graph).stage(0);
    // the acquire rethrows the error of the present before
    try {
        [[maybe_unused]] auto const image = acquire(core, surface, stage);
        ADD_FAILURE() << "the acquire must rethrow the error of the present";
    }
    catch(cvk::vk_result_exception const& e) { EXPECT_EQ(e.vkResult(), VK_ERROR_SURFACE_LOST_KHR); }
    table->vkQueuePresentKHR = realQueuePresent;
    stage.skip();

    // the graph has no error, the surface rethrew it once
    EXPECT_NO_THROW(present_frame(core, graph, surface));
    EXPECT_NO_THROW(present_frame(core, graph, surface));
}

JVK_TEST(Surface, acquire_in_one_stage_presents_after_another) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, scene_and_ui()};

    // the ui stage loads what the scene stage cleared, its epilogue transitions the image
    for(size_t i = 0; i < FRAMES; ++i)
        EXPECT_EQ(present_scene_and_ui(core, graph, surface).extent, test::HEADLESS_EXTENT);
}

JVK_TEST(Surface, present_stage_without_a_use_presents_the_image_of_the_acquire_stage) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, scene_and_ui()};

    // the epilogue of the ui stage starts from the state that the scene stage left
    auto& frame = next_frame(core, graph);
    uint32_t imageIdx = 0;
    {
        auto const image = acquire(core, surface, frame[SCENE], frame[UI]);
        ASSERT_TRUE(image.has_value());
        imageIdx = image->imageIdx;
        auto const scene = frame[SCENE].begin();
        clear(*scene, *image);
    }
    // the scene stage submitted, the image stays acquired until the ui stage
    EXPECT_EQ(frame[SCENE].status(), Stage::Status::SUBMITTED);
    wait(core, frame[SCENE]);
    EXPECT_TRUE(tracker.acquired(surface.swapchain().current(), imageIdx));
    {
        auto const ui = frame[UI].begin();
    }
    EXPECT_TRUE(reaches_unacquired(tracker, surface, imageIdx));

    present_scene_and_ui(core, graph, surface);
}

JVK_TEST(Surface, skipped_present_stage_drops_the_image) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    // one frame slot: the next frame waits for the submission of the skip, its drop included
    auto config = scene_and_ui();
    config.frameSlots = 1;
    StageGraph graph{core, std::move(config)};

    auto& frame = next_frame(core, graph);
    uint32_t imageIdx = 0;
    {
        auto const image = acquire(core, surface, frame[SCENE], frame[UI]);
        ASSERT_TRUE(image.has_value());
        imageIdx = image->imageIdx;
        auto const scene = frame[SCENE].begin();
        clear(*scene, *image);
    }
    frame[UI].skip();
    EXPECT_EQ(acquired_images(tracker, surface), 1);

    // the dropped image comes first, it waits for the skip of the ui stage on the gpu
    auto& next = next_frame(core, graph);
    {
        auto const image = acquire(core, surface, next[SCENE], next[UI]);
        ASSERT_TRUE(image.has_value());
        EXPECT_EQ(image->imageIdx, imageIdx);
        EXPECT_EQ(acquired_images(tracker, surface), 1);
        auto const scene = next[SCENE].begin();
        clear(*scene, *image);
    }
    {
        auto const ui = next[UI].begin();
    }

    present_scene_and_ui(core, graph, surface);
}

JVK_TEST(Surface, exception_in_the_present_stage_drops_the_image) {
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, scene_and_ui()};

    auto& frame = next_frame(core, graph);
    try {
        auto const image = acquire(core, surface, frame[SCENE], frame[UI]);
        ASSERT_TRUE(image.has_value());
        {
            auto const scene = frame[SCENE].begin();
            clear(*scene, *image);
        }
        auto const ui = frame[UI].begin();
        throw test_exception{};
    }
    catch(test_exception const&) {}

    // the scope skipped the ui stage, the skip drops the image, a later acquire gives it back
    EXPECT_EQ(frame[UI].status(), Stage::Status::SKIPPED);
    present_scene_and_ui(core, graph, surface);
    present_scene_and_ui(core, graph, surface);
}

JVK_TEST(Surface, two_surfaces_present_after_one_stage) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto firstWindowSurface = test::headless_surface(context.instance);
    auto secondWindowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, firstWindowSurface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    AcquireTracker const tracker{core};

    Surface first{core, core.queue(0), {}, std::move(firstWindowSurface)};
    Surface second{core, core.queue(0), {}, std::move(secondWindowSurface)};
    StageGraph graph{core, scene_and_ui()};

    // each surface has its scene stage, both present after the ui stage in one present
    for(size_t i = 0; i < FRAMES; ++i) {
        auto& frame = next_frame(core, graph);
        auto const firstImage = acquire(core, first, frame[SCENE], frame[UI]);
        auto const secondImage = acquire(core, second, frame[SCENE], frame[UI]);
        ASSERT_TRUE(firstImage.has_value());
        ASSERT_TRUE(secondImage.has_value());
        {
            auto const scene = frame[SCENE].begin();
            clear(*scene, *firstImage);
            clear(*scene, *secondImage);
        }
        {
            auto const ui = frame[UI].begin();
        }
        wait(core, frame[UI]);
        EXPECT_TRUE(reaches_unacquired(tracker, first, firstImage->imageIdx));
        EXPECT_TRUE(reaches_unacquired(tracker, second, secondImage->imageIdx));
    }
}

JVK_TEST(Surface, present_queue_of_the_same_family) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface, {GRAPHICS, GRAPHICS | PRESENT}))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    if(&core.queue(0) == &core.queue(1)
        || core.queue(0).vk().familyIndex() != core.queue(1).vk().familyIndex())
        GTEST_SKIP() << "needs two vulkan queues of one family, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    Surface surface{core, core.queue(1), {}, std::move(windowSurface)};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    // the submit of the stage signals the present semaphores, the queue of the surface presents
    for(size_t i = 0; i < FRAMES; ++i)
        present_frame(core, graph, surface);
}

JVK_TEST(Surface, present_stage_of_another_family_without_a_use_acquires_the_image) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface, {GRAPHICS | PRESENT, COMPUTE}))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    if(core.queue(0).vk().familyIndex() == core.queue(1).vk().familyIndex())
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    Surface surface{core, core.queue(0), {}, std::move(windowSurface)};
    StageGraph graph{core, scene_and_ui(1)};

    // the scene stage on the family of the surface queue clears the image, the ui stage on the
    // other family presents it without a use: its epilogue acquires the image from the scene queue
    // each acquire waits for the present of the image before
    for(size_t i = 0; i < FRAMES; ++i) {
        auto& frame = next_frame(core, graph);
        auto const image = acquire(core, surface, frame[SCENE], frame[UI]);
        ASSERT_TRUE(image.has_value());
        {
            auto const scene = frame[SCENE].begin();
            clear(*scene, *image);
        }
        auto const ui = frame[UI].begin();
    }
}

JVK_TEST(Surface, present_queue_of_another_family) {
    constexpr size_t FRAMES = 4;
    test::Context const context{};
    auto windowSurface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, windowSurface, {GRAPHICS, COMPUTE | PRESENT}))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto const& stageQueue = core.queue(0);
    auto& surfaceQueue = core.queue(1);
    if(stageQueue.vk().familyIndex() == surfaceQueue.vk().familyIndex())
        GTEST_SKIP() << "needs two queue families, e.g. ANV_QUEUE_OVERRIDE=gc=2,c=1";

    Surface surface{core, surfaceQueue, {}, std::move(windowSurface)};
    {
        StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
        for(size_t i = 0; i < FRAMES; ++i)
            present_frame(core, graph, surface);
    }
    surface.destroy();

    // the epilogue releases the image, one transfer per frame on the queue of the surface acquires
    // it: the next transfer gets the next value of its counter
    auto const next = core.executor().sync_wait(surfaceQueue.transfer(cvk::Barrier{}));
    EXPECT_EQ(next.value, FRAMES + 1);
    surfaceQueue.waitIdle();
}
}
