#include "context.hpp"
#include "test.hpp"

#include "cvk/base/physical_device.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/utility/constant/os_constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <cth/os/wait.hpp>

#include <chrono>
#include <optional>
#include <ranges>


namespace cvk {
namespace {
    VkSubmitInfo2 const EMPTY_SUBMIT{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 0,
        .pWaitSemaphoreInfos = nullptr,
        .commandBufferInfoCount = 0,
        .pCommandBufferInfos = nullptr,
        .signalSemaphoreInfoCount = 0,
        .pSignalSemaphoreInfos = nullptr,
    };

    Core::Config os_fence_config() {
        auto config = test::Context::CONFIG;
        config.extensions.append_range(
            constants::OS_FENCE_DEVICE_EXTENSIONS | std::views::transform(
                [](std::string_view extension) { return std::string{extension}; }
            )
        );
        return config;
    }

    /**
     * @return the device can export @ref constants::OS_FENCE_HANDLE_TYPE
     */
    bool os_fence_exportable(Core const& core) {
        VkPhysicalDeviceExternalFenceInfo const info{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
            .pNext = nullptr,
            .handleType = constants::OS_FENCE_HANDLE_TYPE,
        };
        VkExternalFenceProperties properties{.sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES, .pNext = nullptr};
        vkGetPhysicalDeviceExternalFenceProperties(core.vkPhysicalDevice(), &info, &properties);
        return (properties.externalFenceFeatures & VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT) != 0;
    }
}

CVK_TEST(Fence, base_constructor_does_not_create) {
    test::Context const context{};

    Fence const fence{context.core};

    EXPECT_FALSE(fence.created());
}

CVK_TEST(Fence, create_not_signaled) {
    test::Context const context{};

    Fence const fence{context.core, 0};

    ASSERT_TRUE(fence.created());
    EXPECT_EQ(fence.status(), VK_NOT_READY);
    EXPECT_FALSE(fence.signaled());
    EXPECT_EQ(fence.wait(0), VK_TIMEOUT);
}

CVK_TEST(Fence, create_signaled_and_reset) {
    test::Context const context{};

    Fence const fence{context.core, VK_FENCE_CREATE_SIGNALED_BIT};

    ASSERT_TRUE(fence.signaled());
    fence.wait();

    fence.reset();
    EXPECT_FALSE(fence.signaled());
}

CVK_TEST(Fence, submit_signals) {
    test::Context const context{};
    Fence const fence{context.core, 0};

    context.core.queue(0).raw_submit(EMPTY_SUBMIT, fence.get());

    fence.wait();
    EXPECT_TRUE(fence.signaled());

    fence.waitReset();
    EXPECT_FALSE(fence.signaled());
}

CVK_TEST(Fence, wait_reset_timeout_keeps_the_state) {
    test::Context const context{};
    Fence const fence{context.core, 0};

    EXPECT_EQ(fence.waitReset(0), VK_TIMEOUT);
    EXPECT_FALSE(fence.signaled());

    context.core.queue(0).raw_submit(EMPTY_SUBMIT, fence.get());

    EXPECT_EQ(fence.waitReset(UINT64_MAX), VK_SUCCESS);
    EXPECT_FALSE(fence.signaled());
}

CVK_TEST(Fence, release_and_wrap) {
    test::Context const context{};
    Fence fence{context.core, VK_FENCE_CREATE_SIGNALED_BIT};
    auto const vkFence = fence.get();

    auto const state = fence.release();
    EXPECT_FALSE(fence.created());
    EXPECT_EQ(state.vkFence.get(), vkFence);

    Fence const wrapped{context.core, state};
    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.get(), vkFence);
    EXPECT_TRUE(wrapped.signaled());
}

CVK_TEST(Fence, move_assignment_destroys_the_old_fence) {
    test::Context const context{};
    Fence fence{context.core, 0};
    Fence other{context.core, VK_FENCE_CREATE_SIGNALED_BIT};
    auto const vkFence = other.get();

    fence = std::move(other);

    EXPECT_EQ(fence.get(), vkFence);
    EXPECT_FALSE(other.created());
    EXPECT_TRUE(fence.signaled());
}

CVK_TEST(Fence, extract_os_handle_waits_for_the_submit) {
    std::optional<test::Context> context{};
    try { context.emplace(os_fence_config()); } catch(cvk_exception const&) { GTEST_SKIP() << "no os fence extension"; }
    if(!os_fence_exportable(context->core)) GTEST_SKIP() << "os fence handle type not exportable";

    Fence const fence{context->core, 0, constants::OS_FENCE_HANDLE_TYPE};
    EXPECT_EQ(fence.exportTypes(), constants::OS_FENCE_HANDLE_TYPE);

    auto const& queue = context->core.queue(0);
    queue.raw_submit(EMPTY_SUBMIT, fence.get());

    auto const handle = fence.extractOsHandle();
    if(handle) EXPECT_EQ(cth::os::wait(handle.get(), std::chrono::seconds{5}), cth::os::wait_result::WAITED);

#ifndef CTH_PLATFORM_WINDOWS
    // sync fd export resets the fence
    EXPECT_FALSE(fence.signaled());
#endif

    queue.wait();
}

CVK_TEST(Fence, extract_os_handle_of_a_signaled_fence) {
    std::optional<test::Context> context{};
    try { context.emplace(os_fence_config()); } catch(cvk_exception const&) { GTEST_SKIP() << "no os fence extension"; }
    if(!os_fence_exportable(context->core)) GTEST_SKIP() << "os fence handle type not exportable";

    Fence const fence{context->core, VK_FENCE_CREATE_SIGNALED_BIT, constants::OS_FENCE_HANDLE_TYPE};

    auto const handle = fence.extractOsHandle();

    // invalid handle -> already signaled
    EXPECT_TRUE(!handle || cth::os::check_ready(handle.get()));
}

CVK_TEST(Fence, release_and_wrap_keep_the_export_types) {
    test::Context const context{};
    Fence fence{context.core, 0, constants::OS_FENCE_HANDLE_TYPE};

    auto const state = fence.release();
    EXPECT_EQ(state.exportTypes, constants::OS_FENCE_HANDLE_TYPE);
    EXPECT_EQ(fence.exportTypes(), 0);

    Fence const wrapped{context.core, state};
    EXPECT_EQ(wrapped.exportTypes(), constants::OS_FENCE_HANDLE_TYPE);
}
}
