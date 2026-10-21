#include "headless.hpp"
#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <array>
#include <vector>


namespace cvk {
namespace {
    constexpr std::array PRESENT_QUEUES{QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT};

    /**
     * @brief config that allows everything
     */
    Surface::Config const ALLOW_ALL{.allowedSurfaceFormats = {}, .allowedPresentModes = {}};
}

CVK_TEST(Surface, wrap_takes_ownership) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};

    auto surface = test::headless_surface(instance);

    ASSERT_TRUE(surface.created());
    EXPECT_NE(surface.get(), VK_NULL_HANDLE);

    surface.destroy();
    EXPECT_FALSE(surface.created());
}

CVK_TEST(Surface, release_keeps_the_surface) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    auto surface = test::headless_surface(instance);

    auto const state = surface.release();
    EXPECT_FALSE(surface.created());

    Surface::destroy(instance.get(), state.vkSurface.get());
}

CVK_TEST(Surface, empty_config_allows_everything) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance, ALLOW_ALL)};
    auto const [device, _] = test::pick(instance, PRESENT_QUEUES, surfaces);

    auto const& surface = surfaces[0];

    EXPECT_FALSE(surface.formats(device).empty());
    EXPECT_FALSE(surface.presentModes(device).empty());
    EXPECT_TRUE(std::ranges::contains(surface.presentModes(device), VK_PRESENT_MODE_FIFO_KHR));
}

CVK_TEST(Surface, formats_keep_the_allowed_order) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance, ALLOW_ALL)};
    auto const [device, _] = test::pick(instance, PRESENT_QUEUES, surfaces);

    auto const available = surfaces[0].formats(device);
    if(available.size() < 2) GTEST_SKIP() << "needs two surface formats";

    Surface::Config const config{
        .allowedSurfaceFormats = {available[1], available[0]},
        .allowedPresentModes = {},
    };
    auto const surface = test::headless_surface(instance, config);

    auto const formats = surface.formats(device);
    ASSERT_EQ(formats.size(), 2);
    EXPECT_EQ(formats[0], available[1]);
    EXPECT_EQ(formats[1], available[0]);
    EXPECT_EQ(surface.format(device), available[1]);
}

CVK_TEST(Surface, no_allowed_format_throws) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance, ALLOW_ALL)};
    auto const [device, _] = test::pick(instance, PRESENT_QUEUES, surfaces);

    Surface::Config const config{
        .allowedSurfaceFormats = {{VK_FORMAT_R64G64B64A64_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}},
        .allowedPresentModes = {},
    };
    auto const surface = test::headless_surface(instance, config);

    EXPECT_TRUE(surface.formats(device).empty());
    EXPECT_THROW([[maybe_unused]] auto const f = surface.format(device), cvk_exception);
}

CVK_TEST(Surface, capabilities) {
    Instance const instance{test::instance_config(test::headless_extensions()), create};
    std::array const surfaces{test::headless_surface(instance)};
    auto const [device, _] = test::pick(instance, PRESENT_QUEUES, surfaces);

    auto const capabilities = surfaces[0].capabilities(device);

    EXPECT_GE(capabilities.minImageCount, 1);
}

CVK_TEST(Surface, move_assignment_destroys_the_old_surface) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    test::reset_validation_errors();
    {
        Instance const instance{test::instance_config(test::headless_extensions()), create, test::counting_messenger()};
        auto surface = test::headless_surface(instance);
        auto other = test::headless_surface(instance);
        auto const vkSurface = other.get();

        surface = std::move(other);

        EXPECT_EQ(surface.get(), vkSurface);
        EXPECT_FALSE(other.created());
    }
    EXPECT_EQ(test::validation_errors(), 0);
}
}
