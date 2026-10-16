#include "headless.hpp"
#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <array>
#include <numeric>
#include <vector>


namespace cvk {
using test::APP_NAME;
using test::NO_EXTENSIONS;

namespace {
    /**
     * @brief one queue, some devices have one queue only
     */
    Core::Config const CONFIG{.queueProperties = {QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE}};
    Core::Config const TWO_QUEUES{.queueProperties = {QueueFamilyProperties::GRAPHICS, QueueFamilyProperties::COMPUTE}};

    /**
     * @brief every queue in one group
     */
    Core::queue_set_t one_group(size_t queues) {
        Core::queue_set_t::group_t group(queues);
        std::iota(group.begin(), group.end(), 0uz);
        return Core::queue_set_t{queues, {group}};
    }
}

CVK_TEST(Core, base_constructor_does_not_create) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    Core const core{instance};

    EXPECT_FALSE(core.created());
    EXPECT_EQ(&core.instance(), &instance);
}

CVK_TEST(Core, create_without_queue_sets) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    Core const core{instance, {}, CONFIG};

    ASSERT_TRUE(core.created());
    EXPECT_EQ(core.vkInstance(), instance.get());
    EXPECT_EQ(core.vkPhysicalDevice(), core.physicalDevice().get());
    EXPECT_EQ(core.vkDevice(), core.device().get());
    EXPECT_EQ(core.functions(), core.device().functions());
    EXPECT_EQ(core.deviceTable().device(), core.vkDevice());
    EXPECT_EQ(&core.device().physicalDevice(), &core.physicalDevice());

    ASSERT_EQ(core.queues().size(), CONFIG.queueProperties.size());
    EXPECT_EQ(core.device().queues().size(), CONFIG.queueProperties.size());

    for(size_t i = 0; i < CONFIG.queueProperties.size(); i++) {
        auto const& queue = core.queue(i);
        ASSERT_TRUE(queue.created());
        EXPECT_TRUE(contains(queue.familyProperties(), CONFIG.queueProperties[i]));
        queue.wait();
    }
}

CVK_TEST(Core, create_one_queue_per_entry) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    if(!PhysicalDevice::AutoPick(instance, {}, TWO_QUEUES.queueProperties))
        GTEST_SKIP() << "needs a device with two queues";

    Core const core{instance, {}, TWO_QUEUES};

    ASSERT_EQ(core.queues().size(), 2);
    EXPECT_EQ(core.device().queues().size(), 2);
    EXPECT_NE(core.queue(0).get(), core.queue(1).get());
}

CVK_TEST(Core, queue_set_group_shares_a_queue) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    auto config = TWO_QUEUES;
    config.queueSets.push_back(one_group(config.queueProperties.size()));

    Core const core{instance, {}, config};

    ASSERT_TRUE(core.created());
    EXPECT_EQ(core.queueSetIndex(), 0);
    EXPECT_EQ(core.device().queues().size(), 1);
    EXPECT_EQ(core.queue(0).get(), core.queue(1).get());

    EXPECT_TRUE(contains(core.queue(0).familyProperties(), QueueFamilyProperties::GRAPHICS));
    EXPECT_TRUE(contains(core.queue(0).familyProperties(), QueueFamilyProperties::COMPUTE));
}

CVK_TEST(Core, first_possible_queue_set_wins) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    // no device has this many compute queues
    constexpr size_t queues = 256;

    Core::Config const config{
        .queueProperties = std::vector(queues, QueueFamilyProperties::COMPUTE),
        .queueSets = {Core::queue_set_t{queues}, one_group(queues)},
    };

    Core const core{instance, {}, config};

    ASSERT_TRUE(core.created());
    EXPECT_EQ(core.queueSetIndex(), 1);
    EXPECT_EQ(core.device().queues().size(), 1);
    EXPECT_EQ(core.queues().size(), queues);
}

CVK_TEST(Core, no_suitable_device_throws) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};
    Core::Config const config{.queueProperties = {QueueFamilyProperties::PRESENT}};

    auto const construct = [&] { Core const core{instance, {}, config}; };

    EXPECT_THROW(construct(), cvk_exception);
}

CVK_TEST(Core, release_and_wrap) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    Core core{instance, {}, CONFIG};
    auto const vkDevice = core.vkDevice();

    auto state = core.release();
    EXPECT_FALSE(core.created());
    EXPECT_TRUE(core.queues().empty());

    Core const wrapped{instance, std::move(state)};

    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.vkDevice(), vkDevice);
    ASSERT_EQ(wrapped.queues().size(), CONFIG.queueProperties.size());
    wrapped.queue(0).wait();
}

CVK_TEST(Core, wrap_without_queues_takes_the_device_queues) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    Core core{instance, {}, CONFIG};
    auto state = core.release();
    state.queues.clear();

    Core const wrapped{instance, std::move(state)};

    EXPECT_EQ(wrapped.queues().size(), wrapped.device().queues().size());
}

CVK_TEST(Core, queues_work_after_move) {
    Instance const instance{APP_NAME, NO_EXTENSIONS, std::nullopt};

    Core core{instance, {}, CONFIG};
    Core const moved{std::move(core)};

    EXPECT_FALSE(core.created());
    ASSERT_TRUE(moved.created());
    for(auto const& queue : moved.queues())
        queue.wait();
}

CVK_TEST(Core, present_queue_with_headless_surface) {
    Instance const instance{APP_NAME, test::headless_extensions(), std::nullopt};
    std::array const surfaces{test::headless_surface(instance)};

    Core::Config const config{
        .queueProperties = {QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::PRESENT}
    };

    Core const core{instance, surfaces, config};

    ASSERT_TRUE(core.created());
    EXPECT_TRUE(core.queue(0).can_present());
    EXPECT_TRUE(surfaces[0].supportsFamily(core.physicalDevice(), core.queue(0).familyIndex()));
}
}
