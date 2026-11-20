#include "context.hpp"
#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/vk_overloads.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>


namespace cvk {
CVK_TEST(Surface, wrap_takes_ownership) {
    test::Context const context{};

    auto surface = test::headless_surface(context.instance);

    ASSERT_TRUE(surface.created());
    EXPECT_NE(surface.get(), VK_NULL_HANDLE);

    surface.destroy();
    EXPECT_FALSE(surface.created());
}

CVK_TEST(Surface, release_keeps_the_surface) {
    test::Context const context{};
    auto surface = test::headless_surface(context.instance);

    auto const state = surface.release();
    EXPECT_FALSE(surface.created());

    cvk::destroy(context.instance.get(), state.vkSurface.get());
}

CVK_TEST(Surface, formats_are_every_offered_format) {
    test::PresentContext const context{};
    auto const vkPhysicalDevice = context.core.physicalDevice().get();
    auto const vkSurface = context.surface().get();

    uint32_t count = 0;
    ASSERT_EQ(
        vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface, &count, nullptr),
        VK_SUCCESS
    );
    std::vector<VkSurfaceFormatKHR> offered(count);
    ASSERT_EQ(
        vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface, &count, offered.data()),
        VK_SUCCESS
    );

    auto const formats = context.surface().formats(context.core.physicalDevice());

    EXPECT_FALSE(formats.empty());
    EXPECT_EQ(formats, offered);
}

CVK_TEST(Surface, present_modes_are_every_offered_present_mode) {
    test::PresentContext const context{};
    auto const vkPhysicalDevice = context.core.physicalDevice().get();
    auto const vkSurface = context.surface().get();

    uint32_t count = 0;
    ASSERT_EQ(
        vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, vkSurface, &count, nullptr),
        VK_SUCCESS
    );
    std::vector<VkPresentModeKHR> offered(count);
    ASSERT_EQ(
        vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, vkSurface, &count, offered.data(
        )),
        VK_SUCCESS
    );

    auto const presentModes = context.surface().presentModes(context.core.physicalDevice());

    EXPECT_TRUE(std::ranges::contains(presentModes, VK_PRESENT_MODE_FIFO_KHR));
    EXPECT_EQ(presentModes, offered);
}

CVK_TEST(Surface, capabilities) {
    test::PresentContext const context{};

    auto const capabilities = context.surface().capabilities(context.core.physicalDevice());

    EXPECT_GE(capabilities.minImageCount, 1);
}

CVK_TEST(Surface, extent_comes_from_the_callback_for_an_undefined_current_extent) {
    test::PresentContext const context{};
    auto const& physicalDevice = context.core.physicalDevice();
    auto const capabilities = context.surface().capabilities(physicalDevice);
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        GTEST_SKIP() << "the surface has a fixed extent";

    EXPECT_EQ(context.surface().extent(physicalDevice), test::HEADLESS_EXTENT);
}

CVK_TEST(Surface, extent_of_the_callback_is_clamped_to_the_surface) {
    test::PresentContext const context{};
    auto const& physicalDevice = context.core.physicalDevice();
    static constexpr uint32_t TOO_LARGE_SIDE = std::numeric_limits<uint32_t>::max() - 1;
    static constexpr VkExtent2D TOO_LARGE{TOO_LARGE_SIDE, TOO_LARGE_SIDE};
    auto const surface = test::headless_surface(context.instance, [] { return TOO_LARGE; });
    auto const capabilities = surface.capabilities(physicalDevice);
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        GTEST_SKIP() << "the surface has a fixed extent";

    EXPECT_EQ(surface.extent(physicalDevice), capabilities.maxImageExtent);
}

CVK_TEST(Surface, extent_0_of_the_callback_stays_0) {
    test::PresentContext const context{};
    auto const& physicalDevice = context.core.physicalDevice();
    auto const surface = test::headless_surface(context.instance, [] { return VkExtent2D{0, 32}; });
    if(surface.capabilities(physicalDevice).currentExtent.width != std::numeric_limits<
        uint32_t>::max())
        GTEST_SKIP() << "the surface has a fixed extent";

    EXPECT_EQ(surface.extent(physicalDevice), (VkExtent2D{0, 0}));
}

CVK_TEST(Surface, release_and_wrap_keep_the_extent_callback) {
    test::PresentContext const context{};
    auto const& physicalDevice = context.core.physicalDevice();
    static constexpr VkExtent2D EXTENT{24, 16};
    auto surface = test::headless_surface(context.instance, [] { return EXTENT; });
    if(surface.capabilities(physicalDevice).currentExtent.width != std::numeric_limits<
        uint32_t>::max())
        GTEST_SKIP() << "the surface has a fixed extent";

    Surface const wrapped{context.instance, surface.release()};

    EXPECT_EQ(wrapped.extent(physicalDevice), EXTENT);
}

CVK_TEST(Surface, move_assignment_destroys_the_old_surface) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS)
        GTEST_SKIP() << "needs the validation layers";

    test::InstanceContext const context{test::headless_extensions()};
    auto surface = test::headless_surface(context.instance);
    auto other = test::headless_surface(context.instance);
    auto const vkSurface = other.get();

    surface = std::move(other);

    EXPECT_EQ(surface.get(), vkSurface);
    EXPECT_FALSE(other.created());
}
}
