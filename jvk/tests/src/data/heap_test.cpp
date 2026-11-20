#include "jvk/data/resource_heap.hpp"
#include "jvk/data/sampler_heap.hpp"
#include "context.hpp"
#include "shaders.hpp"
#include "test.hpp"

#include "jvk/base/core.hpp"
#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"
#include "jvk/graph/stage_graph.hpp"
#include "jvk/heap_bindings.hpp"
#include "jvk/utility/exception.hpp"

#include "cvk/render/pipeline/compute_pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"

#include <volk.h>

#include <cth/coro/tasks/executor_task.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>


namespace jvk {
namespace {
    constexpr auto TIMEOUT = std::chrono::seconds{5};

    constexpr cvk::Image::Config TEXTURE_CONFIG{
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    constexpr cvk::Image::Config STORAGE_CONFIG{
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    };

    constexpr std::array<unsigned char, 4> GREEN_PIXEL{0, 255, 0, 255};

    /**
     * the config of the tests with small heaps
     */
    [[nodiscard]] Core::Config small_heaps(uint32_t sampledImages, uint32_t samplers) {
        auto config = test::Context::CONFIG;
        config.heaps = {
            .resources = {
                .sampledImages = sampledImages,
                .storageImages = 0,
                .inputAttachments = 0
            },
            .samplers = {.samplers = samplers},
        };
        return config;
    }

    Frame& next_frame(Core const& core, StageGraph& graph) {
        return core.executor().sync_wait(graph.nextFrame());
    }

    cth::co::executor_task<void> await_stage(Stage const& stage) { co_await stage; }

    cth::co::executor_task<void> await_submitted(Stage const& stage) { co_await stage.submitted(); }

    void wait(Core const& core, Stage const& stage) {
        core.executor().sync_wait(await_stage(stage));
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
     * @return an insert of the descriptor gets the cell before the timeout
     */
    [[nodiscard]] bool reaches_cell(
        ResourceHeap const& heap,
        cvk::SampledImageDescriptor const& descriptor,
        heap_idx<SampledImage> idx
    ) {
        auto const deadline = std::chrono::steady_clock::now() + TIMEOUT;
        while(heap.insert(descriptor).idx() != idx) {
            if(std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        return true;
    }

    [[nodiscard]] bool green(cvk::BaseBuffer const& readback) {
        auto const mapping = readback.map();
        readback.invalidate();
        for(size_t i = 0; i < GREEN_PIXEL.size(); ++i)
            if(std::to_integer<unsigned char>(mapping.span()[i]) != GREEN_PIXEL[i])
                return false;
        return true;
    }
}

JVK_TEST(HeapIdx, is_four_trivially_copyable_bytes) {
    static_assert(sizeof(heap_idx<SampledImage>) == 4);
    static_assert(std::is_trivially_copyable_v<heap_idx<Sampler>>);
    static_assert(!std::is_convertible_v<uint32_t, heap_idx<SampledImage>>);
    static_assert(heap_idx<SampledImage>{3}.get() == 3);
    static_assert(heap_idx<SampledImage>{3} == heap_idx<SampledImage>{3});
}

JVK_TEST(ResourceHeap, handles_are_move_only) {
    static_assert(!std::is_copy_constructible_v<SampledImage>);
    static_assert(std::is_nothrow_move_constructible_v<SampledImage>);
    static_assert(std::is_nothrow_move_assignable_v<StorageImage>);
}

JVK_TEST(ResourceHeap, the_layout_is_fixed_per_handle_type) {
    static_assert(SampledImage::LAYOUT == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    static_assert(StorageImage::LAYOUT == VK_IMAGE_LAYOUT_GENERAL);
    static_assert(InputAttachment::LAYOUT == VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR);
}

JVK_TEST(ResourceHeap, the_destructor_of_a_handle_returns_the_cell) {
    test::Context const context{};
    auto const& heap = context.core.resourceHeap();
    cvk::Image const texture{context.core, TEXTURE_CONFIG, VkExtent2D{1, 1}};

    auto first = heap.insert(cvk::SampledImageDescriptor{texture});
    auto const second = heap.insert(cvk::SampledImageDescriptor{texture});
    EXPECT_NE(first.idx(), second.idx());
    EXPECT_EQ(&first.image(), &texture);

    auto const firstIdx = first.idx();
    {
        auto const gone = std::move(first);
    }
    auto const third = heap.insert(cvk::SampledImageDescriptor{texture});
    EXPECT_EQ(third.idx(), firstIdx);
}

JVK_TEST(ResourceHeap, a_full_area_throws) {
    test::Context const context{};
    Core const core{context.instance, small_heaps(1, 1), test::NO_SURFACES};
    cvk::Image const texture{core, TEXTURE_CONFIG, VkExtent2D{1, 1}};

    auto const only = core.resourceHeap().insert(cvk::SampledImageDescriptor{texture});

    EXPECT_THROW(
        [[maybe_unused]]
        auto const full = core.resourceHeap().insert(cvk::SampledImageDescriptor{texture}),
        jvk_exception
    );
}

JVK_TEST(ResourceHeap, a_retained_cell_comes_back_after_the_gpu_finished) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, test::NO_SURFACES};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const gate{core, cvk::create};
    cvk::Image const texture{core, TEXTURE_CONFIG, VkExtent2D{1, 1}};
    cvk::SampledImageDescriptor const descriptor{texture};
    auto const& heap = core.resourceHeap();

    auto& stage = next_frame(core, graph).stage(0);
    stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    auto handle = heap.insert(descriptor);
    auto const idx = handle.idx();
    {
        auto const primary = stage.begin();
        stage.retain(std::move(handle));
    }
    gate.markSubmitted(1);
    core.executor().sync_wait(await_submitted(stage));

    // the gpu may still use the cell: the next insert takes another one
    EXPECT_NE(heap.insert(descriptor).idx(), idx);

    host_signal(gate, 1);
    wait(core, stage);
    EXPECT_TRUE(reaches_cell(heap, descriptor, idx));
}

JVK_TEST(SamplerHeap, equal_configs_share_a_cell) {
    test::Context const context{};
    auto const& heap = context.core.samplerHeap();

    auto const linear = heap.insert(cvk::SamplerConfig{});
    auto const again = heap.insert(cvk::SamplerConfig{});
    auto const nearest = heap.insert(cvk::SamplerConfig{.magFilter = VK_FILTER_NEAREST});

    EXPECT_EQ(linear.idx(), again.idx());
    EXPECT_NE(linear.idx(), nearest.idx());
}

JVK_TEST(SamplerHeap, the_cell_lives_until_the_last_handle) {
    test::Context const context{};
    Core const core{context.instance, small_heaps(1, 1), test::NO_SURFACES};
    auto const& heap = core.samplerHeap();
    cvk::SamplerConfig const nearest{.magFilter = VK_FILTER_NEAREST};

    std::optional<Sampler> first = heap.insert(cvk::SamplerConfig{});
    std::optional<Sampler> copy = *first;
    EXPECT_EQ(copy->idx(), first->idx());

    first.reset();
    // the copy keeps the only cell
    EXPECT_THROW(
        [[maybe_unused]]
        auto const full = heap.insert(nearest),
        jvk_exception
    );

    copy.reset();
    auto const other = heap.insert(nearest);
    EXPECT_EQ(other.idx().get(), 0);
}

JVK_TEST(SamplerHeap, a_retained_sampler_keeps_its_cell_until_the_gpu_finished) {
    test::Context const context{};
    Core core{context.instance, small_heaps(1, 1), test::NO_SURFACES};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    Semaphore const gate{core, cvk::create};
    auto const& heap = core.samplerHeap();
    cvk::SamplerConfig const nearest{.magFilter = VK_FILTER_NEAREST};

    auto& stage = next_frame(core, graph).stage(0);
    stage.await(gate, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 1);
    {
        auto const primary = stage.begin();
        stage.retain(heap.insert(cvk::SamplerConfig{}));
    }
    gate.markSubmitted(1);
    core.executor().sync_wait(await_submitted(stage));
    EXPECT_THROW(
        [[maybe_unused]]
        auto const full = heap.insert(nearest),
        jvk_exception
    );

    host_signal(gate, 1);
    wait(core, stage);
    auto const deadline = std::chrono::steady_clock::now() + TIMEOUT;
    bool inserted = false;
    while(!inserted && std::chrono::steady_clock::now() < deadline) {
        try {
            [[maybe_unused]] auto const sampler = heap.insert(nearest);
            inserted = true;
        }
        catch(jvk_exception const&) { std::this_thread::sleep_for(std::chrono::milliseconds{1}); }
    }
    EXPECT_TRUE(inserted);
}

JVK_TEST(CmdBuffer, use_of_a_handle_tracks_the_layout_of_its_type_on_the_mip_levels_of_its_view) {
    test::Context const context{};
    auto const& heap = context.core.resourceHeap();
    cvk::Image const image{
        context.core,
        {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
            | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .mipLevels = 3,
        },
        VkExtent2D{4, 4}
    };
    auto const view = [](uint32_t mipLevel) {
        return cvk::ImageViewConfig{.baseMipLevel = mipLevel, .mipLevels = 1};
    };
    auto const sampled = heap.insert(cvk::SampledImageDescriptor{.image = image, .view = view(0)});
    auto const storage = heap.insert(cvk::StorageImageDescriptor{.image = image, .view = view(1)});
    auto const input = heap.insert(cvk::InputAttachmentDescriptor{.image = image, .view = view(2)});

    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};
    primary.begin();
    primary.use(sampled, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    primary.use(
        storage,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
    );
    primary.use(input);

    auto const& states = primary.states();
    EXPECT_EQ(states.last(image, 0)->layout, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    EXPECT_EQ(states.last(image, 0)->scope.access, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    EXPECT_EQ(states.last(image, 1)->layout, VK_IMAGE_LAYOUT_GENERAL);
    EXPECT_EQ(states.last(image, 1)->scope.stages, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    EXPECT_EQ(states.last(image, 2)->layout, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR);
    EXPECT_EQ(states.last(image, 2)->scope.access, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT);
    primary.end();
}

namespace {
    /**
     * texture, target image, their handles and the pipeline of the heap copy shader
     * @details the texture is green after `upload()`, the shader copies it into the target
     */
    struct HeapCopy {
        explicit HeapCopy(Core const& core) : texture{core, TEXTURE_CONFIG, VkExtent2D{1, 1}},
            target{core, STORAGE_CONFIG, VkExtent2D{1, 1}},
            staging{core, test::STAGING_CONFIG, GREEN_PIXEL.size()},
            readback{core, test::READBACK_CONFIG, GREEN_PIXEL.size()},
            sampled{core.resourceHeap().insert(cvk::SampledImageDescriptor{texture})},
            storage{core.resourceHeap().insert(cvk::StorageImageDescriptor{target})},
            sampler{core.samplerHeap().insert(cvk::SamplerConfig{})},
            shader{core, VK_SHADER_STAGE_COMPUTE_BIT, test::shaders::HEAP_COPY_COMP},
            pipeline{
                core,
                {.stage = shader.stageInfo(), .descriptorMappings = mappings(core)},
                cvk::create
            } { staging.write(GREEN_PIXEL); }

        [[nodiscard]] static std::vector<VkDescriptorSetAndBindingMappingEXT> mappings(
            Core const& core
        ) {
            auto mappings = core.resourceHeap().mappings();
            mappings.append_range(core.samplerHeap().mappings());
            return mappings;
        }

        /**
         * records the upload of the texture
         */
        void upload(PrimaryCmdBuffer& cmd) const { texture.copy(cmd, staging); }

        /**
         * records the uses, the pipeline, the indices and the dispatch
         * @pre the heaps are bound or inherited
         */
        template<class CmdBuffer>
        void dispatch(CmdBuffer& cmd) const {
            cmd.use(sampled, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
            cmd.use(
                storage,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
            );
            cmd.bind(pipeline);
            cmd.pushData(std::array{sampled.idx().get(), sampler.idx().get(), storage.idx().get()});
            cmd.dispatch(1);
        }

        /**
         * records the copy of the target into the readback buffer
         */
        void read(PrimaryCmdBuffer& cmd) const {
            target.copyTo(cmd, readback);
            cmd.use(readback, {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT});
        }

        Image texture;
        Image target;
        Buffer<unsigned char> staging;
        Buffer<unsigned char> readback;
        SampledImage sampled;
        StorageImage storage;
        Sampler sampler;
        cvk::Shader shader;
        cvk::ComputePipeline pipeline;
    };
}

JVK_TEST(StageGraph, a_frame_samples_a_texture_and_stores_into_an_image_through_the_heaps) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, test::NO_SURFACES};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};
    HeapCopy const copy{core};

    auto& stage = next_frame(core, graph).stage(0);
    {
        // the begin binds the heaps of the core
        auto const primary = stage.begin();
        copy.upload(*primary);
        copy.dispatch(*primary);
        copy.read(*primary);
    }
    wait(core, stage);

    EXPECT_TRUE(green(copy.readback));
}

JVK_TEST(StageGraph, a_secondary_inherits_the_heaps) {
    test::Context const context{};
    Core core{context.instance, test::Context::CONFIG, test::NO_SURFACES};
    StageGraph graph{core, {.stages = {{.queueIdx = 0, .secondaries = 1}}}};
    HeapCopy const copy{core};

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const primary = stage.begin();
        copy.upload(*primary);
        auto& secondary = stage.beginSecondary(0);
        copy.dispatch(secondary);
        primary->execute(secondary);
        copy.read(*primary);
    }
    wait(core, stage);

    EXPECT_TRUE(green(copy.readback));
}

JVK_TEST(StageGraph, a_core_without_heaps_binds_none) {
    test::Context const context{};
    Core core{context.instance, small_heaps(0, 0), test::NO_SURFACES};
    StageGraph graph{core, {.stages = {{.queueIdx = 0}}}};

    EXPECT_FALSE(core.config().heaps.createsResourceHeap());
    EXPECT_FALSE(core.config().heaps.createsSamplerHeap());

    auto& stage = next_frame(core, graph).stage(0);
    {
        auto const primary = stage.begin();
    }
    wait(core, stage);
}
}
