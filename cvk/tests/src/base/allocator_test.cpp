#include "context.hpp"
#include "test.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/device.hpp"
#include "cvk/base/physical_device.hpp"


namespace cvk {
CVK_TEST(Allocator, core_creates_the_allocator) {
    test::Context const context{};

    auto const& allocator = context.core.allocator();

    EXPECT_TRUE(allocator.created());
    EXPECT_EQ(&allocator.device(), &context.core.device());
}

CVK_TEST(Allocator, base_constructor_does_not_create) {
    test::Context const context{};

    Allocator const allocator{context.core.device()};

    EXPECT_FALSE(allocator.created());
}

CVK_TEST(Allocator, create_and_destroy) {
    test::Context const context{};
    Allocator allocator{context.core.device(), create};

    ASSERT_TRUE(allocator.created());

    allocator.destroy();
    EXPECT_FALSE(allocator.created());
}

CVK_TEST(Allocator, release_and_wrap) {
    test::Context const context{};
    Allocator allocator{context.core.device(), create};
    auto const vmaAllocator = allocator.get();

    auto const state = allocator.release();
    EXPECT_FALSE(allocator.created());

    Allocator const wrapped{context.core.device(), state};
    EXPECT_EQ(wrapped.get(), vmaAllocator);
}

CVK_TEST(Allocator, move_assignment_destroys_the_old_allocator) {
    test::Context const context{};
    Allocator allocator{context.core.device(), create};
    Allocator other{context.core.device(), create};
    auto const vmaAllocator = other.get();

    allocator = std::move(other);

    EXPECT_EQ(allocator.get(), vmaAllocator);
    EXPECT_FALSE(other.created());
}

CVK_TEST(Allocator, flags_follow_the_required_features) {
    auto config = test::Context::CONFIG;
    config.requiredFeatures.features<VkPhysicalDeviceVulkan12Features>().bufferDeviceAddress = VK_TRUE;
    test::Context const context{config};

    auto const flags = Allocator::flags(context.core.physicalDevice());

    EXPECT_NE(flags & VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT, 0u);
}

CVK_TEST(Allocator, no_flags_without_the_features) {
    test::Context const context{};

    auto const flags = Allocator::flags(context.core.physicalDevice());

    EXPECT_EQ(flags & VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT, 0u);
}
}
