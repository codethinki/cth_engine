#include "jvk/cmd/cmd_buffer.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/base/queue.hpp"
#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/queue/queue.hpp"
#include "cvk/base/queue/submit_info.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/sync/barrier.hpp"
#include "cvk/render/sync/fence.hpp"

#include <concepts>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>


namespace jvk {
using test::READBACK_CONFIG;
using test::STAGING_CONFIG;

namespace {
    template<class... Args>
    concept usable = requires(PrimaryCmdBuffer& cmd, Args const&... args) { cmd.use(args...); };

    constexpr VkExtent2D EXTENT{16, 16};

    /**
     * rendering without attachments
     */
    constexpr VkRect2D AREA{.offset = {0, 0}, .extent = EXTENT};

    struct test_exception {};
    constexpr size_t PIXELS = size_t{EXTENT.width} * EXTENT.height;

    constexpr cvk::Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    };

    constexpr cvk::Image::Config COLOR_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    };

    constexpr ImageState UNDEFINED{.layout = VK_IMAGE_LAYOUT_UNDEFINED, .scope = {}};

    constexpr BufferAccess HOST_READ{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};

    /**
     * staging buffer with one counting value per pixel
     */
    [[nodiscard]] Buffer<uint32_t> counting_staging(test::Context const& context) {
        Buffer<uint32_t> staging{context.core, STAGING_CONFIG, PIXELS};
        std::vector<uint32_t> values(PIXELS);
        std::iota(values.begin(), values.end(), uint32_t{0});
        staging.write(values);
        return staging;
    }

    constexpr ImageAccess LOCAL_INPUT_READ{
        .layout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
        .stages = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .access = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
    };

    [[nodiscard]] bool local_read_supported(test::Context const& context) {
        cvk::Core const& core = context.core;
        return core.device().features().dynamicRenderingLocalRead;
    }

    /**
     * color attachment in `VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR`, cleared red
     */
    struct LocalRead {
        static constexpr cvk::Image::Config CONFIG{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        };

        explicit LocalRead(test::Context const& context) :
            image{context.core, CONFIG, EXTENT}, view{image, {}, cvk::create} {}

        [[nodiscard]] cvk::RenderingInfo renderingInfo(VkRenderingFlags flags = 0) const {
            return cvk::RenderingInfo{
                .colorAttachments = {
                    cvk::RenderingAttachment{
                        .view = view,
                        .layout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .clearValue = {.color = {.float32 = {1.f, 0.f, 0.f, 1.f}}},
                    },
                },
                .flags = flags,
            };
        }

        Image image;
        cvk::ImageView view;
    };

    /**
     * @return the readback holds the counting values of `counting_staging()`
     */
    [[nodiscard]] bool counts(cvk::Buffer<uint32_t> const& readback) {
        auto const mapping = readback.map();
        readback.invalidate();
        for(size_t i = 0; i < PIXELS; ++i)
            if(mapping[i] != i) return false;
        return true;
    }
}

JVK_TEST(CmdBuffer, cmd_buffer_concept) {
    static_assert(cmd_buffer<PrimaryCmdBuffer>);
    static_assert(cmd_buffer<SecondaryCmdBuffer>);
    static_assert(!cmd_buffer<cvk::PrimaryCmdBuffer>);
}

JVK_TEST(CmdBuffer, use_takes_pairs_of_resources_and_accesses) {
    using BaseBuffer = cvk::BaseBuffer;

    static_assert(access_pairs<cvk::Image, ImageAccess, BaseBuffer, BufferAccess>);
    static_assert(access_pairs<Image const&, ImageAccess, Buffer<uint32_t>, BufferAccess, Image, ImageAccess>);
    static_assert(!access_pairs<cvk::Image, BufferAccess, BaseBuffer, BufferAccess>);
    static_assert(!access_pairs<BaseBuffer, BufferAccess, BaseBuffer>);
    static_assert(!access_pairs<cvk::Image, ImageAccess>);

    static_assert(usable<cvk::Image, ImageAccess>);
    static_assert(usable<cvk::Image, ImageAccess, BaseBuffer, BufferAccess>);
    static_assert(!usable<cvk::Image, ImageAccess, BaseBuffer, ImageAccess>);
    static_assert(!usable<cvk::Image, ImageAccess, BaseBuffer>);
}

JVK_TEST(CmdBuffer, use_of_several_resources_records_their_barriers) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    auto const staging = counting_staging(context);
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};
    constexpr ImageAccess TRANSFER_DST{
        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    };
    constexpr ImageAccess TRANSFER_SRC{
        .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .access = VK_ACCESS_2_TRANSFER_READ_BIT,
    };
    constexpr BufferAccess TRANSFER_READ{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
    constexpr BufferAccess TRANSFER_WRITE{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};

    primary.begin();
    primary.assume(image, UNDEFINED);
    primary.assume(readback, BufferState{.scope = {.stages = HOST_READ.stages, .access = HOST_READ.access}});

    // the untracked cvk commands rely on the barriers of the use
    primary.use(image, TRANSFER_DST, staging, TRANSFER_READ);
    image.cvk::Image::copy(primary, staging);
    primary.use(image, TRANSFER_SRC, readback, TRANSFER_WRITE);
    image.cvk::Image::copyTo(primary, readback);
    primary.use(readback, HOST_READ);
    primary.end();

    EXPECT_EQ(primary.states().last(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(primary.states().last(staging)->scope.access, VK_ACCESS_2_TRANSFER_READ_BIT);

    test::submit_and_wait(context, primary);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(CmdBuffer, owns_its_pool) {
    test::Context const context{};

    PrimaryCmdBuffer const primary{context.core, test::queue_family(context)};

    ASSERT_TRUE(primary.created());
    EXPECT_EQ(primary.pool().config().flags, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    EXPECT_EQ(primary.pool().config().queueFamilyIndex, test::queue_family(context));
}

JVK_TEST(CmdBuffer, begin_resets_the_states) {
    test::Context const context{};
    cvk::Image const image{context.core, IMAGE_CONFIG, EXTENT};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(image, UNDEFINED);
    EXPECT_FALSE(primary.states().empty());

    // a begin while recording: the pool reset ends the recording
    primary.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    EXPECT_TRUE(primary.recording());
    EXPECT_TRUE(primary.states().empty());
    primary.end();
}

JVK_TEST(CmdBuffer, tracked_copies_are_synchronized) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    auto const staging = counting_staging(context);
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(image, UNDEFINED);
    image.copy(primary, staging);
    image.copyTo(primary, readback);
    primary.use(readback, HOST_READ);
    primary.end();

    auto const last = primary.states().last(image, 0);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    EXPECT_EQ(primary.states().last(readback)->scope.stages, VK_PIPELINE_STAGE_2_HOST_BIT);

    test::submit_and_wait(context, primary);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(CmdBuffer, submit_twice_without_a_new_recording) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    auto const staging = counting_staging(context);
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(image, UNDEFINED);
    image.copy(primary, staging);
    image.copyTo(primary, readback);
    primary.use(readback, HOST_READ);
    primary.end();

    test::submit_and_wait(context, primary);
    test::submit_and_wait(context, primary);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(CmdBuffer, execute_appends_the_states_of_the_secondaries) {
    test::Context const context{};
    Image const image{context.core, IMAGE_CONFIG, EXTENT};
    auto const staging = counting_staging(context);
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};
    SecondaryCmdBuffer first{context.core, test::queue_family(context)};
    SecondaryCmdBuffer second{context.core, test::queue_family(context)};

    // the secondaries do not know the image: their accesses are first uses
    first.begin();
    image.copy(first, staging);
    ASSERT_TRUE(first.states().first(image, 0).has_value());
    EXPECT_EQ(first.states().first(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    second.begin();
    image.copyTo(second, readback);

    primary.begin();
    primary.assume(image, UNDEFINED);
    primary.execute({first, second});
    primary.use(readback, HOST_READ);
    primary.end();

    EXPECT_FALSE(first.recording());
    EXPECT_FALSE(second.recording());
    EXPECT_EQ(primary.states().last(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // the assume came first in the primary
    EXPECT_FALSE(primary.states().first(image, 0).has_value());

    test::submit_and_wait(context, primary);
    EXPECT_TRUE(counts(readback));
}

JVK_TEST(CmdBuffer, begin_rendering_records_the_attachments) {
    test::Context const context{};
    Image const image{context.core, COLOR_CONFIG, EXTENT};
    cvk::ImageView const view{image, {}, cvk::create};
    cvk::Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(image, UNDEFINED);
    auto const rendering = primary.beginRendering({
        .colorAttachments = {
            cvk::RenderingAttachment{
                .view = view,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {.color = {.float32 = {1.f, 0.f, 0.f, 1.f}}},
            },
        },
    });
    primary.endRendering();

    auto const last = primary.states().last(image, 0);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(last->scope.stages, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(last->scope.access, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    image.copyTo(primary, readback);
    primary.use(readback, HOST_READ);
    primary.end();

    test::submit_and_wait(context, primary);

    auto const mapping = readback.map();
    readback.invalidate();
    EXPECT_EQ(mapping[0], 0xff0000ffu);
}

JVK_TEST(CmdBuffer, rendering_scope_ends_the_rendering_at_its_end) {
    test::Context const context{};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    {
        auto const rendering = primary.beginRendering({.area = AREA});
        EXPECT_NE(primary.rendering(), nullptr);
    }
    EXPECT_EQ(primary.rendering(), nullptr);

    // an exception ends it too
    try {
        auto const rendering = primary.beginRendering({.area = AREA});
        throw test_exception{};
    }
    catch(test_exception const&) {}
    EXPECT_EQ(primary.rendering(), nullptr);
    primary.end();

    test::submit_and_wait(context, primary);
}

JVK_TEST(CmdBuffer, rendering_scope_does_nothing_after_an_end_or_a_move) {
    test::Context const context{};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    // a second end fails its check
    primary.begin();
    {
        auto rendering = primary.beginRendering({.area = AREA});
        rendering.end();
        EXPECT_EQ(primary.rendering(), nullptr);
    }

    // ended by the command buffer: the rendering is not open at the end
    {
        auto const rendering = primary.beginRendering({.area = AREA});
        primary.endRendering();
    }

    // the moved-from scope does nothing
    {
        auto rendering = primary.beginRendering({.area = AREA});
        auto const moved = std::move(rendering);
    }
    EXPECT_EQ(primary.rendering(), nullptr);
    primary.end();

    test::submit_and_wait(context, primary);
}

JVK_TEST(CmdBuffer, use_inside_a_rendering_records_a_local_read_barrier) {
    test::Context const context{};
    if(!local_read_supported(context)) GTEST_SKIP() << "needs dynamicRenderingLocalRead";
    LocalRead const localRead{context};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(localRead.image, UNDEFINED);
    auto const rendering = primary.beginRendering(localRead.renderingInfo());
    // a write of the attachment, then a read through the input attachment: by region barrier, no layout change
    primary.use(localRead.image, LOCAL_INPUT_READ);
    EXPECT_EQ(primary.states().last(localRead.image, 0)->scope.access, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT);
    primary.endRendering();

    // the end adds the store of the attachment
    auto const last = primary.states().last(localRead.image, 0);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->layout, VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR);
    EXPECT_EQ(last->scope.access, VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    primary.end();

    test::submit_and_wait(context, primary);
}

JVK_TEST(CmdBuffer, copy_after_a_rendering_with_a_local_read) {
    test::Context const context{};
    if(!local_read_supported(context)) GTEST_SKIP() << "needs dynamicRenderingLocalRead";
    LocalRead const localRead{context};
    auto const staging = counting_staging(context);
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(localRead.image, UNDEFINED);
    auto const rendering = primary.beginRendering(localRead.renderingInfo());
    primary.use(localRead.image, LOCAL_INPUT_READ);
    primary.endRendering();

    // read after the store, then write after the read
    localRead.image.copyTo(primary, readback);
    localRead.image.copy(primary, staging);
    primary.use(readback, HOST_READ);
    primary.end();

    test::submit_and_wait(context, primary);

    auto const mapping = readback.map();
    readback.invalidate();
    EXPECT_EQ(mapping[0], 0xff0000ffu);
}

JVK_TEST(CmdBuffer, continuing_secondary_records_a_local_read_barrier) {
    test::Context const context{};
    if(!local_read_supported(context)) GTEST_SKIP() << "needs dynamicRenderingLocalRead";
    LocalRead const localRead{context};
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, PIXELS};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};
    SecondaryCmdBuffer secondary{context.core, test::queue_family(context)};

    primary.begin();
    primary.assume(localRead.image, UNDEFINED);
    auto const rendering = primary.beginRendering(
        localRead.renderingInfo(VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT)
    );

    secondary.begin(primary.inheritance());
    EXPECT_TRUE(secondary.insideRendering());
    // the secondary does not know the attachment write of the primary
    secondary.assume(
        localRead.image,
        {
            .layout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
            .scope = {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
        }
    );
    secondary.use(localRead.image, LOCAL_INPUT_READ);

    primary.execute(secondary);
    primary.endRendering();
    localRead.image.copyTo(primary, readback);
    primary.use(readback, HOST_READ);
    primary.end();

    test::submit_and_wait(context, primary);

    auto const mapping = readback.map();
    readback.invalidate();
    EXPECT_EQ(mapping[0], 0xff0000ffu);
}

JVK_TEST(CmdBuffer, bind_records_the_buffer_access) {
    test::Context const context{};
    auto const staging = counting_staging(context);
    Buffer<uint32_t> const vertices{
        context.core,
        {.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT},
        PIXELS
    };
    Buffer<uint32_t> const indices{
        context.core,
        {.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT},
        PIXELS
    };
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    vertices.copy(primary, staging);
    indices.copy(primary, staging);
    primary.bindVertices(0, vertices);
    primary.bindIndices(indices);
    primary.end();

    EXPECT_EQ(primary.states().last(vertices)->scope.access, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);
    EXPECT_EQ(primary.states().last(indices)->scope.access, VK_ACCESS_2_INDEX_READ_BIT);
    EXPECT_EQ(primary.states().first(vertices)->scope.access, VK_ACCESS_2_TRANSFER_WRITE_BIT);

    test::submit_and_wait(context, primary);
}

JVK_TEST(CmdBuffer, converts_to_the_const_cvk_type) {
    static_assert(std::convertible_to<PrimaryCmdBuffer&, cvk::PrimaryCmdBuffer const&>);
    static_assert(std::convertible_to<SecondaryCmdBuffer&, cvk::SecondaryCmdBuffer const&>);
    static_assert(std::convertible_to<PrimaryCmdBuffer const&, cvk::BaseCmdBuffer const&>);
    static_assert(std::convertible_to<SecondaryCmdBuffer const&, cvk::BaseCmdBuffer const&>);
    static_assert(!std::convertible_to<PrimaryCmdBuffer&, cvk::PrimaryCmdBuffer&>);
    static_assert(!std::convertible_to<SecondaryCmdBuffer&, cvk::BaseCmdBuffer&>);
}

JVK_TEST(CmdBuffer, submit_info_takes_the_jvk_buffer) {
    test::Context const context{};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};
    primary.begin();
    primary.end();

    cvk::SubmitInfo const info{.cmdBuffers = {primary}};
    ASSERT_EQ(info.cmdBuffers.size(), 1);
    EXPECT_EQ(&info.cmdBuffers[0].get(), &static_cast<cvk::PrimaryCmdBuffer const&>(primary));

    cvk::Fence const fence{context.core, cvk::create};
    context.core.queue(0).vk().submit(info, &fence);
    fence.wait();
}

JVK_TEST(CmdBuffer, cvk_function_records_untracked) {
    test::Context const context{};
    cvk::Image const image{context.core, IMAGE_CONFIG, EXTENT};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    cvk::Barrier barrier{};
    barrier.addTransition(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED);
    // cvk::Barrier::execute() takes cvk::BaseCmdBuffer const&
    barrier.execute(primary);
    EXPECT_TRUE(primary.states().empty());

    primary.assume(
        image,
        {
            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .scope = {VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT},
        }
    );
    primary.end();

    EXPECT_EQ(primary.states().last(image, 0)->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    test::submit_and_wait(context, primary);
}

JVK_TEST(CmdBuffer, move_keeps_the_pool) {
    test::Context const context{};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};
    auto const vkBuffer = primary.get();

    PrimaryCmdBuffer moved{std::move(primary)};
    moved.begin();
    moved.end();

    EXPECT_EQ(moved.get(), vkBuffer);

    PrimaryCmdBuffer other{context.core, test::queue_family(context)};
    other = std::move(moved);
    EXPECT_EQ(other.get(), vkBuffer);
}
}
