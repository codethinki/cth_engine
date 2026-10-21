#include "context.hpp"
#include "test.hpp"

#include "cgfw/window/window.hpp"

#include "cvk/surface/surface.hpp"

#include <cth/os/osdef.hpp>

#include <algorithm>
#include <array>
#include <string>


namespace cgfw {
CGFW_TEST(Window, base_constructor_does_not_create) {
    Window const window{test::window_config()};

    EXPECT_FALSE(window.created());
    EXPECT_EQ(window.get(), nullptr);
    EXPECT_EQ(window.config().title, test::window_config().title);
}

CGFW_TEST(Window, create_and_destroy) {
    CGFW_SKIP_WITHOUT_DISPLAY();
    Window window{test::window_config()};

    window.create();
    ASSERT_TRUE(window.created());
    EXPECT_NE(window.get(), nullptr);

    window.destroy();
    EXPECT_FALSE(window.created());
}

CGFW_TEST(Window, constructor_creates) {
    CGFW_SKIP_WITHOUT_DISPLAY();

    Window const window{test::window_config(), create};

    EXPECT_TRUE(window.created());
    EXPECT_FALSE(window.shouldClose());
}

CGFW_TEST(Window, release_and_wrap) {
    CGFW_SKIP_WITHOUT_DISPLAY();
    Window window{test::window_config(), create};
    auto const rgfwWindow = window.get();

    auto const state = window.release();
    EXPECT_FALSE(window.created());
    EXPECT_EQ(state.rgfwWindow.get(), rgfwWindow);

    Window const wrapped{test::window_config(), state};
    EXPECT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), rgfwWindow);
}

CGFW_TEST(Window, move_keeps_the_window) {
    CGFW_SKIP_WITHOUT_DISPLAY();
    Window window{test::window_config(), create};
    auto const rgfwWindow = window.get();

    Window const moved{std::move(window)};

    EXPECT_FALSE(window.created());
    EXPECT_EQ(moved.get(), rgfwWindow);
    moved.pollEvents();
    EXPECT_FALSE(moved.shouldClose());
}

CGFW_TEST(Window, move_assignment_destroys_the_old_window) {
    CGFW_SKIP_WITHOUT_DISPLAY();
    Window window{test::window_config("old"), create};
    Window other{test::window_config("new"), create};
    auto const rgfwWindow = other.get();

    window = std::move(other);

    EXPECT_EQ(window.get(), rgfwWindow);
    EXPECT_EQ(window.config().title, "new");
    EXPECT_FALSE(other.created());
}

CGFW_TEST(Window, windows_exist_at_the_same_time) {
    CGFW_SKIP_WITHOUT_DISPLAY();

    std::array<Window, 3> const windows{
        Window{test::window_config("first"), create},
        Window{test::window_config("second"), create},
        Window{test::window_config("third"), create},
    };

    windows[0].pollEvents();
    for(auto const& window : windows) {
        EXPECT_TRUE(window.created());
        EXPECT_FALSE(window.shouldClose());
    }
    EXPECT_NE(windows[0].get(), windows[1].get());
    EXPECT_NE(windows[1].get(), windows[2].get());
    EXPECT_NE(windows[0].get(), windows[2].get());
}

CGFW_TEST(Window, surface_of_an_instance_with_the_extensions) {
    CGFW_SKIP_WITHOUT_DISPLAY();
    test::InstanceContext const context{};
    Window const window{test::window_config(), create};

    auto const surface = window.surface(context.instance);

    EXPECT_TRUE(surface.created());
}

CGFW_TEST(instance_extensions, contain_the_surface_extensions) {
    auto const extensions = instance_extensions();

    ASSERT_FALSE(extensions.empty());
    EXPECT_TRUE(std::ranges::contains(extensions, std::string{VK_KHR_SURFACE_EXTENSION_NAME}));
#ifdef CTH_PLATFORM_WINDOWS
    EXPECT_TRUE(std::ranges::contains(extensions, std::string{"VK_KHR_win32_surface"}));
#else
    EXPECT_TRUE(std::ranges::contains(extensions, std::string{"VK_KHR_wayland_surface"}));
#endif
}
}
