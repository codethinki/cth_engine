#include "context.hpp"
#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>


namespace cvk {
using test::APP_NAME;

CVK_TEST(Instance, base_constructor_does_not_create) {
    Instance const instance{test::instance_config()};

    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, create_and_destroy) {
    test::Context::ValidationCheck const validationCheck{};
    Instance instance{test::instance_config({}, test::counting_messenger())};

    instance.create();
    ASSERT_TRUE(instance.created());
    EXPECT_EQ(volkGetLoadedInstance(), instance.get());

    instance.destroy();
    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, constructor_creates) {
    test::Context::ValidationCheck const validationCheck{};

    Instance const instance{test::instance_config({}, test::counting_messenger()), create};

    EXPECT_TRUE(instance.created());
}

CVK_TEST(Instance, create_twice_loads_the_new_instance) {
    test::InstanceContext context{};

    context.instance.create();

    ASSERT_TRUE(context.instance.created());
    EXPECT_EQ(volkGetLoadedInstance(), context.instance.get());
}

CVK_TEST(Instance, create_enables_the_extensions_of_the_config) {
    std::string const surface{VK_KHR_SURFACE_EXTENSION_NAME};

    test::InstanceContext const context{{surface}};

    auto const& instance = context.instance;
    EXPECT_TRUE(std::ranges::contains(instance.extensions(), surface));
}

CVK_TEST(Instance, create_with_a_messenger_config_creates_the_messenger) {
    // also release builds: the config decides, not the validation layers
    test::Context::ValidationCheck const validationCheck{};
    std::string const debugUtils{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

    Instance const instance{test::instance_config({}, test::counting_messenger()), create};

    ASSERT_NE(instance.debugMessenger(), nullptr);
    EXPECT_EQ(instance.debugMessenger()->config().callback, test::counting_messenger().callback);
    EXPECT_TRUE(std::ranges::contains(instance.extensions(), debugUtils));
}

CVK_TEST(Instance, create_without_a_messenger_config) {
    std::string const debugUtils{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};

    Instance const instance{test::instance_config(), create};

    if constexpr(constants::ENABLE_VALIDATION_LAYERS) {
        // debug builds always have a messenger: the default one
        ASSERT_NE(instance.debugMessenger(), nullptr);
        EXPECT_EQ(instance.debugMessenger()->config().callback, &default_debug_callback);
        EXPECT_TRUE(std::ranges::contains(instance.extensions(), debugUtils));
    } else {
        EXPECT_EQ(instance.debugMessenger(), nullptr);
        EXPECT_FALSE(std::ranges::contains(instance.extensions(), debugUtils));
    }
}

CVK_TEST(Instance, create_with_a_missing_extension_throws_and_forgets) {
    // a created instance with the config of the missing extension: create() destroys it first
    test::InstanceContext context{};
    Instance instance{
        test::instance_config({"VK_CVK_extension_that_does_not_exist"}, test::counting_messenger()),
        context.instance.release()
    };
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
    // debug builds always have a messenger: the state needs one
    std::array<char const*, 1> const extensions{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    VkInstanceCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };
    VkInstance vkInstance = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateInstance(&createInfo, nullptr, &vkInstance), VK_SUCCESS);
    volkLoadInstanceOnly(vkInstance);

    auto const messengerInfo = test::counting_messenger().createInfo();
    VkDebugUtilsMessengerEXT vkMessenger = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateDebugUtilsMessengerEXT(vkInstance, &messengerInfo, nullptr, &vkMessenger), VK_SUCCESS);

    Instance instance{
        test::instance_config({}, test::counting_messenger()),
        Instance::State{.vkInstance = vkInstance, .debugMessenger = DebugMessenger::State{vkMessenger}}
    };

    ASSERT_TRUE(instance.created());
    EXPECT_EQ(instance.get(), vkInstance);
    EXPECT_EQ(volkGetLoadedInstance(), vkInstance);
    ASSERT_NE(instance.debugMessenger(), nullptr);
    EXPECT_EQ(instance.debugMessenger()->get(), vkMessenger);

    instance.destroy();
    EXPECT_FALSE(instance.created());
}

CVK_TEST(Instance, move_assignment_takes_the_other_instance) {
    test::InstanceContext context{};
    Instance other{test::instance_config({}, test::counting_messenger()), create};
    auto const vkInstance = other.get();

    context.instance = std::move(other);

    EXPECT_EQ(context.instance.get(), vkInstance);
    EXPECT_FALSE(other.created());
}

CVK_TEST(Instance, release_and_wrap) {
    test::InstanceContext context{};
    auto& instance = context.instance;
    auto const vkInstance = instance.get();
    ASSERT_NE(instance.debugMessenger(), nullptr);
    auto const vkMessenger = instance.debugMessenger()->get();

    auto const extensions = std::vector<std::string>{std::from_range, instance.extensions()};

    auto state = instance.release();
    EXPECT_FALSE(instance.created());
    EXPECT_EQ(instance.debugMessenger(), nullptr);
    EXPECT_EQ(state.vkInstance.get(), vkInstance);
    EXPECT_EQ(state.extensions, extensions);
    ASSERT_TRUE(state.debugMessenger.has_value());
    EXPECT_EQ(state.debugMessenger->vkMessenger.get(), vkMessenger);

    Instance const wrapped{test::instance_config({}, test::counting_messenger()), std::move(state)};
    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkInstance);
    EXPECT_TRUE(std::ranges::equal(wrapped.extensions(), extensions));
    ASSERT_NE(wrapped.debugMessenger(), nullptr);
    EXPECT_EQ(wrapped.debugMessenger()->get(), vkMessenger);
}
}
