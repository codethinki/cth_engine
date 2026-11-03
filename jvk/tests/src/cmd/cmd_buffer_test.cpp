#include "jvk/cmd/cmd_buffer.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/data/buffer.hpp"
#include "jvk/data/image.hpp"

#include "cvk/data/img/image_view.hpp"
#include "cvk/render/sync/barrier.hpp"

#include <array>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>


namespace jvk {
using test::READBACK_CONFIG;
using test::STAGING_CONFIG;

namespace {
    constexpr VkExtent2D EXTENT{16, 16};
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

    /**
     * @return the readback holds the counting values of @ref counting_staging()
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
    std::array<SecondaryCmdBuffer*, 2> const secondaries{&first, &second};
    primary.execute(secondaries);
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
    primary.beginRendering({
        .colorAttachments = {
            cvk::RenderingAttachment{
                .view = &view,
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
    primary.bindVertexBuffer(0, vertices);
    primary.bindIndexBuffer(indices);
    primary.end();

    EXPECT_EQ(primary.states().last(vertices)->scope.access, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT);
    EXPECT_EQ(primary.states().last(indices)->scope.access, VK_ACCESS_2_INDEX_READ_BIT);
    EXPECT_EQ(primary.states().first(vertices)->scope.access, VK_ACCESS_2_TRANSFER_WRITE_BIT);

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
