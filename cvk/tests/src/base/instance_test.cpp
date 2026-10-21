#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <string>
#include <vector>


namespace cvk {
using test::APP_NAME;

CVK_TEST(Instance, base_constructor_does_not_create) {
    Instance const instance{test::instance_config()};

    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, create_and_destroy) {
    Instance instance{test::instance_config()};

    instance.create();
    ASSERT_TRUE(instance.created());
    EXPECT_EQ(volkGetLoadedInstance(), instance.get());

    instance.destroy();
    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, constructor_creates) {
    Instance const instance{test::instance_config(), create};

    EXPECT_TRUE(instance.created());
}

CVK_TEST(Instance, create_twice_loads_the_new_instance) {
    Instance instance{test::instance_config(), create};

    instance.create();

    ASSERT_TRUE(instance.created());
    EXPECT_EQ(volkGetLoadedInstance(), instance.get());
}

CVK_TEST(Instance, create_enables_the_extensions_of_the_config) {
    std::string const surface{VK_KHR_SURFACE_EXTENSION_NAME};
    Instance const instance{test::instance_config({surface}), create};

    EXPECT_TRUE(std::ranges::contains(instance.extensions(), surface));
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        std::string const debugUtils{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
        EXPECT_TRUE(std::ranges::contains(instance.extensions(), debugUtils));
        EXPECT_NE(instance.debugMessenger(), nullptr);
    }
}

CVK_TEST(Instance, create_with_a_missing_extension_throws_and_forgets) {
    // a created instance with the config of the missing extension: create() destroys it first
    Instance created{test::instance_config(), create};
    Instance instance{test::instance_config({"VK_CVK_extension_that_does_not_exist"}), created.release()};
    ASSERT_TRUE(instance.created());

    EXPECT_THROW(instance.create(), cvk_exception);
    EXPECT_FALSE(instance.created());
    EXPECT_TRUE(instance.extensions().empty());
    EXPECT_EQ(instance.debugMessenger(), nullptr);
}

CVK_TEST(Instance, available_extensions_contain_the_surface) {
    std::string const surface{VK_KHR_SURFACE_EXTENSION_NAME};
    EXPECT_TRUE(std::ranges::contains(available_instance_extensions(), surface));
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

    Instance instance{test::instance_config(), Instance::State{vkInstance}};

    ASSERT_TRUE(instance.created());
    EXPECT_EQ(instance.get(), vkInstance);
    EXPECT_EQ(volkGetLoadedInstance(), vkInstance);

    instance.destroy();
    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, move_assignment_takes_the_other_instance) {
    test::reset_validation_errors();
    {
        Instance instance{test::instance_config(), create, test::counting_messenger()};
        Instance other{test::instance_config(), create, test::counting_messenger()};
        auto const vkInstance = other.get();

        instance = std::move(other);

        EXPECT_EQ(instance.get(), vkInstance);
        EXPECT_FALSE(other.created());
    }
    EXPECT_EQ(test::validation_errors(), 0);
}

CVK_TEST(Instance, release_and_wrap) {
    Instance instance{test::instance_config(), create};
    auto const vkInstance = instance.get();

    auto const extensions = std::vector<std::string>{std::from_range, instance.extensions()};

    auto state = instance.release();
    EXPECT_FALSE(instance.created());
    EXPECT_EQ(instance.debugMessenger(), nullptr);
    EXPECT_EQ(state.vkInstance.get(), vkInstance);
    EXPECT_EQ(state.extensions, extensions);

    Instance const wrapped{test::instance_config(), std::move(state)};
    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkInstance);
    EXPECT_TRUE(std::ranges::equal(wrapped.extensions(), extensions));
    if constexpr(constants::ENABLE_VALIDATION_LAYERS) EXPECT_NE(wrapped.debugMessenger(), nullptr);
}
}
