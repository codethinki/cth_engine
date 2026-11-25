#include "jvk/graph/presenter.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/base/queue.hpp"
#include "jvk/cmd/cmd_buffer.hpp"
#include "jvk/sync/semaphore.hpp"

#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/surface/swapchain.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <volk.h>

#include <cth/ptr/ref_wrapper.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <ranges>
#include <semaphore>
#include <thread>
#include <utility>
#include <variant>
#include <vector>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    constexpr auto TIMEOUT = std::chrono::seconds{5};
    /** a wait that ends in this time counts as not pending */
    constexpr auto PENDING = std::chrono::milliseconds{50};

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

    /**
     * @return the acquire semaphore of the wait of the image, `nullptr` -> a ready point
     */
    [[nodiscard]] cvk::Semaphore const* acquire_semaphore(Presenter::Image const& image) {
        auto const* const semaphorePtr =
            std::get_if<cth::ref_wrapper<cvk::Semaphore const>>(&image.wait());
        return semaphorePtr != nullptr ? &semaphorePtr->get() : nullptr;
    }

    /**
     * adds the wait of the image and the gate to the submit
     * @param gatePtr `nullptr` -> no gate, else the submit waits for its value `1`
     */
    void add_waits(SubmitInfo& info, Presenter::Image const& image, Semaphore const* gatePtr) {
        if(gatePtr != nullptr)
            info.waits.push_back({.semaphore = *gatePtr, .value = 1});
        if(auto const* const semaphorePtr = acquire_semaphore(image))
            info.binaryWaits.emplace_back(*semaphorePtr);
        else
            info.waits.push_back(std::get<SemaphoreSubmitInfo>(image.wait()));
    }

    /**
     * the submits of a test
     * @details the destructor waits until the queue is idle: the command buffers and the
     *  semaphores go after the gpu
     */
    struct Submits {
        Submits(Core const& core, Queue& queue) :
            core{core},
            queue{queue},
            done{core, cvk::create} {}
        ~Submits() { queue->waitIdle(); }

        /**
         * transitions the image to `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` after its wait, signals its
         * present semaphore
         * @param gatePtr `nullptr` -> no gate, else the submit waits for its value `1` too
         */
        void transition(Presenter::Image const& image, Semaphore const* gatePtr = nullptr) {
            auto& cmd = cmds.emplace_back(*core, queue->vk().familyIndex(), cvk::create);
            cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            cvk::Barrier barrier{};
            barrier.add(
                image.image(),
                cvk::ImageBarrierInfo{
                    .src = {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
                    .dst = {.stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT},
                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                }
            );
            barrier.execute(cmd);
            cmd.end();

            SubmitInfo info{
                .cmdBuffers = {cmd},
                .signals = {{.semaphore = done, .value = done.next()}},
                .binarySignals = {image.presentSemaphore()},
            };
            add_waits(info, image, gatePtr);
            core->executor().sync_wait(queue->submit(info));
        }

        /**
         * waits for the image without a use
         * @param gatePtr `nullptr` -> no gate, else the submit waits for its value `1` too
         * @return the signal of the submit, the ready point of a drop
         */
        [[nodiscard]] SemaphoreSubmitInfo ready(
            Presenter::Image const& image,
            Semaphore const* gatePtr = nullptr
        ) {
            SemaphoreSubmitInfo const signal{.semaphore = done, .value = done.next()};
            SubmitInfo info{.signals = {signal}};
            add_waits(info, image, gatePtr);
            core->executor().sync_wait(queue->submit(info));
            return signal;
        }

        /**
         * transitions and presents the image
         */
        void present(Presenter& presenter, Presenter::Image image) {
            transition(image);
            core->executor().sync_wait(presenter.present(std::move(image)));
        }

        cth::ref_wrapper<Core const> core;
        cth::ref_wrapper<Queue> queue;
        Semaphore done;
        std::deque<PrimaryCmdBuffer> cmds{};
    };

    [[nodiscard]] std::optional<Presenter::Image> acquire(Core const& core, Presenter& presenter) {
        return core.executor().sync_wait(presenter.acquire());
    }

    /** the `vkQueuePresentKHR()` of the device table before the hook */
    PFN_vkQueuePresentKHR realQueuePresent = nullptr;

    /** the images of the presents through `recording_present()`, in present order */
    std::vector<uint32_t> presentedIndices{};

    VKAPI_ATTR VkResult VKAPI_CALL recording_present(VkQueue queue, VkPresentInfoKHR const* info) {
        for(uint32_t i = 0; i < info->swapchainCount; ++i)
            presentedIndices.push_back(info->pImageIndices[i]);
        return realQueuePresent(queue, info);
    }

    /**
     * presents, then reports the result for every swapchain
     */
    template<VkResult Result>
    VKAPI_ATTR VkResult VKAPI_CALL failing_present(VkQueue queue, VkPresentInfoKHR const* info) {
        if(auto const result = realQueuePresent(queue, info); result < 0)
            return result;
        if(info->pResults != nullptr)
            for(uint32_t i = 0; i < info->swapchainCount; ++i)
                info->pResults[i] = Result;
        return Result;
    }

    /** count of the calls of `out_of_date_acquire()` */
    std::atomic<size_t> outOfDateAcquires = 0;

    VKAPI_ATTR VkResult VKAPI_CALL out_of_date_acquire(
        VkDevice,
        VkSwapchainKHR,
        uint64_t,
        VkSemaphore,
        VkFence,
        uint32_t*
    ) {
        ++outOfDateAcquires;
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    PFN_vkGetFenceStatus realGetFenceStatus = nullptr;
    PFN_vkWaitForFences realWaitForFences = nullptr;

    /** `true` -> `held_fence_status()` reports every fence as unsignaled */
    std::atomic<bool> fencesHeld = false;
    /** count of the calls of `releasing_fence_wait()` */
    std::atomic<size_t> fenceWaits = 0;

    VKAPI_ATTR VkResult VKAPI_CALL held_fence_status(VkDevice device, VkFence fence) {
        return fencesHeld ? VK_NOT_READY : realGetFenceStatus(device, fence);
    }

    /**
     * ends the hold of the fences, then waits
     */
    VKAPI_ATTR VkResult VKAPI_CALL releasing_fence_wait(
        VkDevice device,
        uint32_t fenceCount,
        VkFence const* fences,
        VkBool32 waitAll,
        uint64_t timeout
    ) {
        ++fenceWaits;
        fencesHeld = false;
        return realWaitForFences(device, fenceCount, fences, waitAll, timeout);
    }

    /**
     * the device table of the core, it reads the table at each call
     */
    [[nodiscard]] VolkDeviceTable& device_table(Core const& core) {
        return *const_cast<VolkDeviceTable*>(static_cast<cvk::Core const&>(core).functions());
    }

    /**
     * replaces a function in the device table of the core until the destruction
     */
    template<class Pfn>
    struct Hook {
        Hook(Pfn& function, Pfn hook, Pfn& real) : function{function}, real{real} {
            real = std::exchange(function, hook);
        }
        ~Hook() { function.get() = real.get(); }

        cth::ref_wrapper<Pfn> function;
        cth::ref_wrapper<Pfn> real;

        Hook(Hook const& other) = delete;
        Hook& operator=(Hook const& other) = delete;
    };

    /**
     * replaces `vkQueuePresentKHR()` of the core until the destruction
     */
    struct PresentHook : Hook<PFN_vkQueuePresentKHR> {
        PresentHook(Core const& core, PFN_vkQueuePresentKHR hook) :
            Hook{device_table(core).vkQueuePresentKHR, hook, realQueuePresent} {}
    };
}

JVK_TEST(Presenter, acquire_and_present_round_trips) {
    constexpr size_t FRAMES = 8;
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    EXPECT_EQ(presenter.framesInFlight(), 2);
    {
        Submits submits{core, queue};
        for(size_t i = 0; i < FRAMES; ++i) {
            auto imageOpt = acquire(core, presenter);
            ASSERT_TRUE(imageOpt.has_value());
            EXPECT_EQ(imageOpt->acquireNumber(), i + 1);
            EXPECT_EQ(imageOpt->extent(), test::HEADLESS_EXTENT);
            EXPECT_LT(imageOpt->imageIdx(), swapchain.imageCount());
            EXPECT_EQ(&imageOpt->image(), &swapchain.image(imageOpt->imageIdx()));
            // a new image waits for its acquire
            EXPECT_NE(acquire_semaphore(*imageOpt), nullptr);
            submits.present(presenter, std::move(*imageOpt));
        }
    }

    // the queue is idle: the present fences signaled
    EXPECT_TRUE(presenter.finished());
}

JVK_TEST(Presenter, presents_follow_the_acquire_order) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    Submits submits{core, queue};
    presentedIndices.clear();
    PresentHook const hook{core, recording_present};

    auto firstOpt = acquire(core, presenter);
    auto secondOpt = acquire(core, presenter);
    ASSERT_TRUE(firstOpt.has_value());
    ASSERT_TRUE(secondOpt.has_value());
    auto const firstIdx = firstOpt->imageIdx();
    auto const secondIdx = secondOpt->imageIdx();
    submits.transition(*firstOpt);
    submits.transition(*secondOpt);

    // the second image is handed back first, its present waits for the first image
    std::binary_semaphore presentedSignal{0};
    std::jthread presenting{
        [&] {
            core.executor().sync_wait(presenter.present(std::move(*secondOpt)));
            presentedSignal.release();
        }
    };
    EXPECT_FALSE(presentedSignal.try_acquire_for(PENDING));

    core.executor().sync_wait(presenter.present(std::move(*firstOpt)));
    EXPECT_TRUE(presentedSignal.try_acquire_for(TIMEOUT));
    presenting.join();

    EXPECT_EQ(presentedIndices, (std::vector{firstIdx, secondIdx}));
}

JVK_TEST(Presenter, drop_spares_the_image_with_its_acquire_semaphore) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    Submits submits{core, queue};

    auto droppedOpt = acquire(core, presenter);
    ASSERT_TRUE(droppedOpt.has_value());
    auto const imageIdx = droppedOpt->imageIdx();
    auto const* const semaphorePtr = acquire_semaphore(*droppedOpt);
    presenter.drop(std::move(*droppedOpt));

    // the spare comes first, with the wait of the drop
    auto spareOpt = acquire(core, presenter);
    ASSERT_TRUE(spareOpt.has_value());
    EXPECT_EQ(spareOpt->imageIdx(), imageIdx);
    EXPECT_EQ(acquire_semaphore(*spareOpt), semaphorePtr);
    EXPECT_EQ(spareOpt->acquireNumber(), 2);
    submits.present(presenter, std::move(*spareOpt));
}

JVK_TEST(Presenter, drop_with_a_ready_point_spares_the_image_behind_it) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    // one acquire semaphore: the acquire after the drop reuses it once the ready point is reached
    Presenter presenter{queue, swapchain, 1};
    Submits submits{core, queue};
    Semaphore const gate{core, cvk::create};

    auto droppedOpt = acquire(core, presenter);
    ASSERT_TRUE(droppedOpt.has_value());
    auto const imageIdx = droppedOpt->imageIdx();
    auto const ready = submits.ready(*droppedOpt, &gate);
    presenter.drop(std::move(*droppedOpt), ready);
    EXPECT_FALSE(presenter.finished());

    // the spare waits for the ready point
    auto spareOpt = acquire(core, presenter);
    ASSERT_TRUE(spareOpt.has_value());
    EXPECT_EQ(spareOpt->imageIdx(), imageIdx);
    ASSERT_TRUE(std::holds_alternative<SemaphoreSubmitInfo>(spareOpt->wait()));
    auto const& wait = std::get<SemaphoreSubmitInfo>(spareOpt->wait());
    EXPECT_EQ(&*wait.semaphore, &*ready.semaphore);
    EXPECT_EQ(wait.value, ready.value);

    // the gate before the present: the present waits for no signal that the gpu holds back
    host_signal(gate, 1);
    submits.present(presenter, std::move(*spareOpt));

    // the acquire semaphore is free once the gpu reached the ready point
    auto nextOpt = acquire(core, presenter);
    ASSERT_TRUE(nextOpt.has_value());
    EXPECT_NE(acquire_semaphore(*nextOpt), nullptr);
    submits.present(presenter, std::move(*nextOpt));
}

JVK_TEST(Presenter, ready_point_of_the_last_acquire_semaphore_holds_the_acquire_back) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 1};
    Submits submits{core, queue};
    Semaphore const gate{core, cvk::create};

    auto droppedOpt = acquire(core, presenter);
    ASSERT_TRUE(droppedOpt.has_value());
    auto const ready = submits.ready(*droppedOpt, &gate);
    presenter.drop(std::move(*droppedOpt), ready);
    auto spareOpt = acquire(core, presenter);
    ASSERT_TRUE(spareOpt.has_value());

    // no spare, the only acquire semaphore waits for the ready point
    std::optional<Presenter::Image> nextOpt{};
    std::binary_semaphore acquiredSignal{0};
    std::jthread acquiring{
        [&] {
            nextOpt = acquire(core, presenter);
            acquiredSignal.release();
        }
    };
    EXPECT_FALSE(acquiredSignal.try_acquire_for(PENDING));

    host_signal(gate, 1);
    EXPECT_TRUE(acquiredSignal.try_acquire_for(TIMEOUT));
    acquiring.join();
    ASSERT_TRUE(nextOpt.has_value());
    EXPECT_NE(nextOpt->imageIdx(), spareOpt->imageIdx());

    submits.present(presenter, std::move(*spareOpt));
    submits.present(presenter, std::move(*nextOpt));
}

JVK_TEST(Presenter, destroyed_image_is_dropped) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    Submits submits{core, queue};

    uint32_t firstIdx = 0;
    {
        auto imageOpt = acquire(core, presenter);
        ASSERT_TRUE(imageOpt.has_value());
        firstIdx = imageOpt->imageIdx();
    }
    auto firstOpt = acquire(core, presenter);
    ASSERT_TRUE(firstOpt.has_value());
    EXPECT_EQ(firstOpt->imageIdx(), firstIdx);

    // the move assignment drops the image it replaces
    auto secondOpt = acquire(core, presenter);
    ASSERT_TRUE(secondOpt.has_value());
    auto const secondIdx = secondOpt->imageIdx();
    *secondOpt = std::move(*firstOpt);
    EXPECT_EQ(secondOpt->imageIdx(), firstIdx);
    auto spareOpt = acquire(core, presenter);
    ASSERT_TRUE(spareOpt.has_value());
    EXPECT_EQ(spareOpt->imageIdx(), secondIdx);

    submits.present(presenter, std::move(*secondOpt));
    submits.present(presenter, std::move(*spareOpt));
}

JVK_TEST(Presenter, destructor_releases_the_spares) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    auto const imageCount = swapchain.imageCount();
    {
        Presenter presenter{queue, swapchain, 2};
        Submits submits{core, queue};

        // a spare with its acquire semaphore and a spare behind a ready point
        auto firstOpt = acquire(core, presenter);
        auto secondOpt = acquire(core, presenter);
        ASSERT_TRUE(firstOpt.has_value());
        ASSERT_TRUE(secondOpt.has_value());
        auto const ready = submits.ready(*secondOpt);
        presenter.drop(std::move(*firstOpt));
        presenter.drop(std::move(*secondOpt), ready);
    }

    // the spares are back in the swapchain: the next presenter acquires every image
    Presenter presenter{queue, swapchain, imageCount};
    std::vector<Presenter::Image> images{};
    for(uint32_t i = 0; i < imageCount; ++i) {
        auto imageOpt = acquire(core, presenter);
        ASSERT_TRUE(imageOpt.has_value());
        images.push_back(std::move(*imageOpt));
    }
    // the destructor of the images drops them, the destructor of the presenter releases them
}

JVK_TEST(Presenter, not_ready_try_falls_back_until_a_hand_back) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    auto const imageCount = swapchain.imageCount();
    // a semaphore for every image and one for the acquire that finds none
    Presenter presenter{queue, swapchain, imageCount + 1};
    Submits submits{core, queue};

    std::vector<Presenter::Image> images{};
    for(uint32_t i = 0; i < imageCount; ++i) {
        auto imageOpt = acquire(core, presenter);
        ASSERT_TRUE(imageOpt.has_value());
        images.push_back(std::move(*imageOpt));
    }

    // every image is acquired: the try returns VK_NOT_READY, the fallback does not block
    std::optional<Presenter::Image> nextOpt{};
    std::binary_semaphore acquiredSignal{0};
    std::jthread acquiring{
        [&] {
            nextOpt = acquire(core, presenter);
            acquiredSignal.release();
        }
    };
    EXPECT_FALSE(acquiredSignal.try_acquire_for(PENDING));

    auto const firstIdx = images.front().imageIdx();
    submits.present(presenter, std::move(images.front()));
    EXPECT_TRUE(acquiredSignal.try_acquire_for(TIMEOUT));
    acquiring.join();
    ASSERT_TRUE(nextOpt.has_value());
    // the headless surface gives the presented image back at once
    EXPECT_EQ(nextOpt->imageIdx(), firstIdx);

    for(auto& image : images | std::views::drop(1))
        submits.present(presenter, std::move(image));
    submits.present(presenter, std::move(*nextOpt));
}

JVK_TEST(Presenter, fallback_waits_for_the_present_fence_of_the_last_acquire_semaphore) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 1};
    Submits submits{core, queue};

    auto firstOpt = acquire(core, presenter);
    ASSERT_TRUE(firstOpt.has_value());
    submits.present(presenter, std::move(*firstOpt));

    // the present fence counts as unsignaled until a wait for it: no try, the fallback waits
    auto& table = device_table(core);
    fencesHeld = true;
    fenceWaits = 0;
    std::optional<Presenter::Image> nextOpt{};
    {
        Hook const status{table.vkGetFenceStatus, held_fence_status, realGetFenceStatus};
        Hook const wait{table.vkWaitForFences, releasing_fence_wait, realWaitForFences};
        nextOpt = acquire(core, presenter);
    }
    EXPECT_EQ(fenceWaits, 1);
    ASSERT_TRUE(nextOpt.has_value());
    EXPECT_NE(acquire_semaphore(*nextOpt), nullptr);
    submits.present(presenter, std::move(*nextOpt));
}

JVK_TEST(Presenter, failed_present_arrives_once_at_the_next_acquire) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    Submits submits{core, queue};

    {
        PresentHook const hook{core, failing_present<VK_ERROR_SURFACE_LOST_KHR>};
        auto imageOpt = acquire(core, presenter);
        ASSERT_TRUE(imageOpt.has_value());
        // the present does not throw
        EXPECT_NO_THROW(submits.present(presenter, std::move(*imageOpt)));
    }

    try {
        [[maybe_unused]] auto const imageOpt = acquire(core, presenter);
        ADD_FAILURE() << "the acquire must rethrow the failure of the present";
    }
    catch(cvk::vk_result_exception const& e) { EXPECT_EQ(e.vkResult(), VK_ERROR_SURFACE_LOST_KHR); }

    auto imageOpt = acquire(core, presenter);
    ASSERT_TRUE(imageOpt.has_value());
    submits.present(presenter, std::move(*imageOpt));
}

JVK_TEST(Presenter, out_of_date_present_gives_no_image_from_then_on) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    Submits submits{core, queue};

    {
        PresentHook const hook{core, failing_present<VK_ERROR_OUT_OF_DATE_KHR>};
        auto imageOpt = acquire(core, presenter);
        ASSERT_TRUE(imageOpt.has_value());
        submits.present(presenter, std::move(*imageOpt));
    }

    EXPECT_FALSE(acquire(core, presenter).has_value());
    EXPECT_FALSE(acquire(core, presenter).has_value());
}

JVK_TEST(Presenter, out_of_date_acquire_gives_no_image_from_then_on) {
    test::Context const context{};
    auto const surface = test::headless_surface(context.instance);
    std::optional<Core> coreOpt{};
    if(!create_present_core(coreOpt, context, surface))
        GTEST_SKIP() << "no device presents to a headless surface";
    auto& core = *coreOpt;
    auto& queue = core.queue(0);

    cvk::Swapchain swapchain{core, queue.vk(), surface, {}, test::HEADLESS_EXTENT};
    Presenter presenter{queue, swapchain, 2};
    Submits submits{core, queue};

    auto imageOpt = acquire(core, presenter);
    ASSERT_TRUE(imageOpt.has_value());

    outOfDateAcquires = 0;
    {
        PFN_vkAcquireNextImageKHR real = nullptr;
        Hook const hook{device_table(core).vkAcquireNextImageKHR, out_of_date_acquire, real};
        EXPECT_FALSE(acquire(core, presenter).has_value());
        // out of date stays: no second try
        EXPECT_FALSE(acquire(core, presenter).has_value());
    }
    EXPECT_EQ(outOfDateAcquires, 1);

    // the image acquired before still presents
    submits.present(presenter, std::move(*imageOpt));
}
}
