#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/debug/debug_messenger.hpp"

#include <string_view>


namespace cvk {
namespace {
    constexpr std::string_view TEST_MESSAGE = "cvk test message";

    size_t received = 0;

    VKAPI_ATTR VkBool32 VKAPI_CALL count_test_messages(
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
        void*
    ) {
        if(callbackData->pMessage == TEST_MESSAGE) ++received;
        return VK_FALSE;
    }

    void submit_test_message(Instance const& instance) {
        VkDebugUtilsMessengerCallbackDataEXT const data{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
            .pNext = nullptr,
            .flags = 0,
            .pMessageIdName = nullptr,
            .messageIdNumber = 0,
            .pMessage = TEST_MESSAGE.data(),
            .queueLabelCount = 0,
            .pQueueLabels = nullptr,
            .cmdBufLabelCount = 0,
            .pCmdBufLabels = nullptr,
            .objectCount = 0,
            .pObjects = nullptr,
        };

        vkSubmitDebugUtilsMessageEXT(
            instance.get(),
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
            &data
        );
    }
}

CVK_TEST(DebugMessenger, default_config_create_info) {
    constexpr auto info = DebugMessenger::Config{}.createInfo();

    static_assert(info.sType == VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
    static_assert(info.pfnUserCallback == &default_debug_callback);
    static_assert(info.messageSeverity == constants::DEBUG_MESSAGE_SEVERITY);
    static_assert(info.messageType == constants::DEBUG_MESSAGE_TYPE);
}

CVK_TEST(DebugMessenger, forwards_messages_to_the_callback) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{test::instance_config(), create};
    DebugMessenger const messenger{instance, DebugMessenger::Config{.callback = count_test_messages}, create};
    ASSERT_TRUE(messenger.created());

    received = 0;
    submit_test_message(instance);

    EXPECT_EQ(received, 1);
}

CVK_TEST(DebugMessenger, accepts_a_lambda) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{test::instance_config(), create};

    DebugMessenger::callback_t const callback = [](
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
        void*
    ) -> VkBool32 {
        if(callbackData->pMessage == TEST_MESSAGE) ++received;
        return VK_FALSE;
    };
    DebugMessenger const messenger{instance, DebugMessenger::Config{.callback = callback}, create};

    received = 0;
    submit_test_message(instance);

    EXPECT_EQ(received, 1);
}

CVK_TEST(DebugMessenger, destroy_resets) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{test::instance_config(), create};
    DebugMessenger messenger{instance, DebugMessenger::Config{.callback = count_test_messages}, create};

    messenger.destroy();
    EXPECT_FALSE(messenger.created());

    received = 0;
    submit_test_message(instance);

    EXPECT_EQ(received, 0);
}

CVK_TEST(DebugMessenger, release_keeps_the_messenger) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{test::instance_config(), create};
    DebugMessenger messenger{instance, DebugMessenger::Config{.callback = count_test_messages}, create};

    auto const state = messenger.release();
    EXPECT_FALSE(messenger.created());

    received = 0;
    submit_test_message(instance);
    EXPECT_EQ(received, 1);

    DebugMessenger::destroy(instance.get(), state.vkMessenger.get());
}

CVK_TEST(DebugMessenger, release_and_wrap) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{test::instance_config(), create};
    DebugMessenger messenger{instance, DebugMessenger::Config{.callback = count_test_messages}, create};
    auto const vkMessenger = messenger.get();

    auto const state = messenger.release();
    DebugMessenger const wrapped{instance, messenger.config(), state};

    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkMessenger);

    received = 0;
    submit_test_message(instance);
    EXPECT_EQ(received, 1);
}

CVK_TEST(DebugMessenger, move_assignment_destroys_the_old_messenger) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    test::reset_validation_errors();
    {
        Instance const instance{test::instance_config(), create, test::counting_messenger()};
        DebugMessenger messenger{instance, DebugMessenger::Config{.callback = count_test_messages}, create};
        DebugMessenger other{instance, DebugMessenger::Config{.callback = count_test_messages}, create};
        auto const vkMessenger = other.get();

        messenger = std::move(other);

        EXPECT_EQ(messenger.get(), vkMessenger);
        EXPECT_FALSE(other.created());

        received = 0;
        submit_test_message(instance);
        EXPECT_EQ(received, 1);
    }
    EXPECT_EQ(test::validation_errors(), 0);
}
}
