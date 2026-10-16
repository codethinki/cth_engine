#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/debug/debug_messenger.hpp"

#include <string_view>


namespace cvk {
using test::APP_NAME;
using test::NO_EXTENSIONS;

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

CVK_TEST(DebugMessenger, forwards_messages_to_the_callback) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    DebugMessenger const messenger{DebugMessenger::Config::Default(count_test_messages), instance};
    ASSERT_TRUE(messenger.created());

    received = 0;
    submit_test_message(instance);

    EXPECT_EQ(received, 1);
}

CVK_TEST(DebugMessenger, accepts_a_lambda) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    DebugMessenger::callback_t const callback = [](
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
        void*
    ) -> VkBool32 {
        if(callbackData->pMessage == TEST_MESSAGE) ++received;
        return VK_FALSE;
    };
    DebugMessenger const messenger{DebugMessenger::Config::Default(callback), instance};

    received = 0;
    submit_test_message(instance);

    EXPECT_EQ(received, 1);
}

CVK_TEST(DebugMessenger, destroy_resets) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    DebugMessenger messenger{DebugMessenger::Config::Default(count_test_messages), instance};

    messenger.destroy();
    EXPECT_FALSE(messenger.created());

    received = 0;
    submit_test_message(instance);

    EXPECT_EQ(received, 0);
}

CVK_TEST(DebugMessenger, release_keeps_the_messenger) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    DebugMessenger messenger{DebugMessenger::Config::Default(count_test_messages), instance};

    auto const state = messenger.release();
    EXPECT_FALSE(messenger.created());

    received = 0;
    submit_test_message(instance);
    EXPECT_EQ(received, 1);

    DebugMessenger::destroy(state.vkInstance, state.vkMessenger.get());
}

CVK_TEST(DebugMessenger, move_assignment_destroys_the_old_messenger) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    test::reset_validation_errors();
    {
        Instance const instance{APP_NAME, NO_EXTENSIONS, test::counting_messenger()};
        DebugMessenger messenger{DebugMessenger::Config::Default(count_test_messages), instance};
        DebugMessenger other{DebugMessenger::Config::Default(count_test_messages), instance};
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
