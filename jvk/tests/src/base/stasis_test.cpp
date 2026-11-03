#include "jvk/base/stasis.hpp"

#include "context.hpp"
#include "test.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/sync/semaphore.hpp"

#include <utility>


namespace jvk {
namespace {
    constexpr VkExtent2D EXTENT{16, 16};

    constexpr cvk::Image::Config IMAGE_CONFIG{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    template<class State>
    concept addable = requires(Stasis& stasis, State&& state) { stasis.add(std::forward<State>(state)); };
}

JVK_TEST(Stasis, takes_rvalue_states_only) {
    static_assert(addable<cvk::Semaphore::State>);
    static_assert(!addable<cvk::Semaphore::State&>);
    static_assert(!addable<cvk::Instance::State>);
}

JVK_TEST(Stasis, context_comes_from_the_core) {
    test::Context const context{};
    cvk::Core const& core = context.core;

    Stasis const stasis{context.core};

    EXPECT_EQ(stasis.table().device(), core.vkDevice());
    EXPECT_EQ(stasis.allocator(), core.allocator().get());
    EXPECT_TRUE(stasis.empty());
}

JVK_TEST(Stasis, clear_destroys_every_handle) {
    // a leak shows up when the shared device is destroyed
    test::Context const context{};
    Stasis stasis{context.core};
    cvk::Semaphore semaphore{context.core, cvk::create};
    cvk::Image image{context.core, IMAGE_CONFIG, EXTENT};
    cvk::CmdPool pool{context.core, {.queueFamilyIndex = test::queue_family(context)}, cvk::create};
    cvk::PrimaryCmdBuffer primary{pool, cvk::create};

    // the pool before its command buffer: the pool children are destroyed first
    stasis.add(primary.release());
    stasis.add(pool.release());
    stasis.add(semaphore.release());
    stasis.add(image.release());

    // the image with its memory
    EXPECT_EQ(stasis.size(), 5);

    stasis.clear();
    EXPECT_TRUE(stasis.empty());
}

JVK_TEST(Stasis, skips_empty_optionals) {
    test::Context const context{};
    Stasis stasis{context.core};
    cvk::Image image{context.core, IMAGE_CONFIG, EXTENT};
    auto state = image.release();

    // another owner frees the memory, after the image
    cvk::Memory const memory{context.core, IMAGE_CONFIG.memory, *state.memory};
    state.memory = std::nullopt;

    stasis.add(std::move(state));

    EXPECT_EQ(stasis.size(), 1);
    stasis.clear();
}

JVK_TEST(Stasis, destructor_destroys_the_handles) {
    test::Context const context{};
    cvk::Semaphore semaphore{context.core, cvk::create};

    Stasis stasis{context.core};
    stasis.add(semaphore.release());
}

JVK_TEST(Stasis, move_assignment_destroys_the_old_handles) {
    test::Context const context{};
    cvk::Semaphore semaphore{context.core, cvk::create};
    cvk::Semaphore other{context.core, cvk::create};
    Stasis stasis{context.core};
    Stasis next{context.core};
    stasis.add(semaphore.release());
    next.add(other.release());

    stasis = std::move(next);

    EXPECT_EQ(stasis.size(), 1);
}
}
