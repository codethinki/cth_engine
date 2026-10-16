#include "test.hpp"

#include "cvk/base/instance.hpp"

#include <algorithm>
#include <string>
#include <vector>


namespace cvk {
using test::APP_NAME;
using test::NO_EXTENSIONS;

CVK_TEST(Instance, base_constructor_does_not_create) {
    Instance const instance{APP_NAME, NO_EXTENSIONS};

    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, create_and_destroy) {
    Instance instance{APP_NAME, NO_EXTENSIONS};

    instance.create();
    ASSERT_TRUE(instance.created());
    EXPECT_EQ(volkGetLoadedInstance(), instance.get());

    instance.destroy();
    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, constructor_creates) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    EXPECT_TRUE(instance.created());
}

CVK_TEST(Instance, create_twice_loads_the_new_instance) {
    Instance instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    instance.create();

    ASSERT_TRUE(instance.created());
    EXPECT_EQ(volkGetLoadedInstance(), instance.get());
}

CVK_TEST(Instance, reports_required_extensions) {
    std::vector<std::string> const required{VK_KHR_SURFACE_EXTENSION_NAME};

    Instance const instance{APP_NAME, required};

    EXPECT_TRUE(std::ranges::contains(instance.requiredExtensions(), std::string{VK_KHR_SURFACE_EXTENSION_NAME}));
}

CVK_TEST(Instance, missing_extension_throws) {
    std::vector<std::string> const required{"VK_CVK_extension_that_does_not_exist"};

    auto const construct = [&] { Instance const instance{APP_NAME, required}; };

    EXPECT_THROW(construct(), cth::except::default_exception);
}

CVK_TEST(Instance, wrap_takes_ownership) {
    VkApplicationInfo const appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = APP_NAME.data(),
        .applicationVersion = 0,
        .pEngineName = APP_NAME.data(),
        .engineVersion = 0,
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };
    VkInstance vkInstance = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateInstance(&createInfo, nullptr, &vkInstance), VK_SUCCESS);

    Instance instance{APP_NAME, NO_EXTENSIONS, Instance::State{vkInstance, nullptr}};

    ASSERT_TRUE(instance.created());
    EXPECT_EQ(instance.get(), vkInstance);
    EXPECT_EQ(volkGetLoadedInstance(), vkInstance);

    instance.destroy();
    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, move_assignment_takes_the_other_instance) {
    Instance instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    Instance other{APP_NAME, NO_EXTENSIONS, std::nullopt};
    auto const vkInstance = other.get();

    instance = std::move(other);

    EXPECT_EQ(instance.get(), vkInstance);
    EXPECT_FALSE(other.created());
}
}
