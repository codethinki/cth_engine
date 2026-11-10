#include "context.hpp"
#include "test.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/descriptor/descriptor_pool.hpp"
#include "cvk/data/descriptor/descriptor_set.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/render/cmd/cmd_buffer.hpp"
#include "cvk/render/cmd/cmd_pool.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constant/device_constants.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <numeric>
#include <span>
#include <vector>


namespace cvk {
namespace {
    using test::Context;

    Core::Config const TWO_QUEUES{.queueProperties = {QueueFamilyProperties::GRAPHICS, QueueFamilyProperties::COMPUTE}};

    /**
     * every queue in one group
     */
    Core::queue_set_t one_group(size_t queues) {
        Core::queue_set_t::group_t group(queues);
        std::iota(group.begin(), group.end(), 0uz);
        return Core::queue_set_t{queues, {group}};
    }
}

CVK_TEST(Core, base_constructor_does_not_create) {
    Context const context{};

    Core const core{context.instance};

    EXPECT_FALSE(core.created());
    EXPECT_EQ(&core.instance(), &context.instance);
}

CVK_TEST(Core, create_without_queue_sets) {
    Context const context{};

    Core const core{context.instance, {}, Context::CONFIG};

    ASSERT_TRUE(core.created());
    EXPECT_EQ(core.vkInstance(), context.instance.get());
    EXPECT_EQ(core.vkPhysicalDevice(), core.physicalDevice().get());
    EXPECT_EQ(core.vkDevice(), core.device().get());
    EXPECT_EQ(core.functions(), core.device().functions());
    EXPECT_EQ(core.deviceTable().device(), core.vkDevice());
    EXPECT_EQ(&core.device().physicalDevice(), &core.physicalDevice());

    ASSERT_EQ(core.queueIndices().size(), Context::CONFIG.queueProperties.size());
    EXPECT_EQ(core.device().queues().size(), Context::CONFIG.queueProperties.size());

    for(size_t i = 0; i < Context::CONFIG.queueProperties.size(); i++) {
        auto const& queue = core.queue(i);
        ASSERT_NE(queue.get(), VK_NULL_HANDLE);
        EXPECT_TRUE(contains(queue.familyProperties(), Context::CONFIG.queueProperties[i]));
        queue.waitIdle();
    }
}

CVK_TEST(Core, create_one_queue_per_entry) {
    Context const context{};

    if(!pick_physical_device(context.instance, {}, TWO_QUEUES.queueProperties, constants::BASE_DEVICE_FEATURES))
        GTEST_SKIP() << "needs a device with two queues";

    Core const core{context.instance, {}, TWO_QUEUES};

    ASSERT_EQ(core.queueIndices().size(), 2);
    EXPECT_EQ(core.device().queues().size(), 2);
    EXPECT_NE(core.queueIndex(0), core.queueIndex(1));
    EXPECT_NE(core.queue(0).get(), core.queue(1).get());
}

CVK_TEST(Core, queue_set_group_shares_a_queue) {
    Context const context{};

    auto config = TWO_QUEUES;
    config.queueSets.push_back(one_group(config.queueProperties.size()));

    Core const core{context.instance, {}, config};

    ASSERT_TRUE(core.created());
    EXPECT_EQ(core.queueSetIndex(), 0);
    EXPECT_EQ(core.device().queues().size(), 1);
    EXPECT_EQ(core.queueIndex(0), core.queueIndex(1));
    EXPECT_EQ(&core.queue(0), &core.queue(1));

    EXPECT_TRUE(contains(core.queue(0).familyProperties(), QueueFamilyProperties::GRAPHICS));
    EXPECT_TRUE(contains(core.queue(0).familyProperties(), QueueFamilyProperties::COMPUTE));
}

CVK_TEST(Core, first_possible_queue_set_wins) {
    Context const context{};

    // no device has this many compute queues
    constexpr size_t queues = 256;

    Core::Config const config{
        .queueProperties = std::vector(queues, QueueFamilyProperties::COMPUTE),
        .queueSets = {Core::queue_set_t{queues}, one_group(queues)},
    };

    Core const core{context.instance, {}, config};

    ASSERT_TRUE(core.created());
    EXPECT_EQ(core.queueSetIndex(), 1);
    EXPECT_EQ(core.device().queues().size(), 1);
    EXPECT_EQ(core.queueIndices().size(), queues);
}

CVK_TEST(Core, no_suitable_device_throws) {
    Context const context{};
    Core::Config const config{.queueProperties = {QueueFamilyProperties::PRESENT}};

    auto const construct = [&] { Core const core{context.instance, {}, config}; };

    EXPECT_THROW(construct(), cvk_exception);
}

CVK_TEST(Core, device_uuid_picks_exactly_this_device) {
    Context const context{};
    auto config = Context::CONFIG;

    // every suitable device, not only the one that the automatic pick takes
    for(auto const& device : enumerate_physical_devices(context.instance)) {
        bool const suitable = device.missing(constants::BASE_DEVICE_FEATURES).empty()
            && !device.queueFamilyIndices(config.queueProperties).empty();
        if(!suitable) continue;

        config.device = device.uuid();
        Core const core{context.instance, {}, config};

        EXPECT_EQ(core.physicalDevice().uuid(), device.uuid());
        EXPECT_EQ(core.vkPhysicalDevice(), device.get());
    }
}

CVK_TEST(Core, missing_device_uuid_throws) {
    Context const context{};
    device_uuid_t unknown{};
    unknown.fill(0xFF);

    Core::Config const withoutSets{.queueProperties = Context::CONFIG.queueProperties, .device = unknown};
    Core::Config const withSets{
        .queueProperties = Context::CONFIG.queueProperties,
        .queueSets = {Core::queue_set_t{1}},
        .device = unknown,
    };

    auto const construct = [&](Core::Config const& config) { Core const core{context.instance, {}, config}; };

    EXPECT_THROW(construct(withoutSets), cvk_exception);
    EXPECT_THROW(construct(withSets), cvk_exception);
}

CVK_TEST(Core, release_and_wrap) {
    Context const context{};

    Core core{context.instance, {}, Context::CONFIG};
    auto const vkDevice = core.vkDevice();
    auto const vmaAllocator = core.allocator().get();

    auto state = core.release();
    EXPECT_FALSE(core.created());
    EXPECT_TRUE(core.queueIndices().empty());

    Core const wrapped{context.instance, std::move(state)};

    ASSERT_TRUE(wrapped.created());
    EXPECT_EQ(wrapped.vkDevice(), vkDevice);
    EXPECT_EQ(wrapped.allocator().get(), vmaAllocator);
    ASSERT_EQ(wrapped.queueIndices().size(), Context::CONFIG.queueProperties.size());
    wrapped.queue(0).waitIdle();
}

CVK_TEST(Core, wrap_without_queue_indices_takes_the_device_queues) {
    Context const context{};

    Core core{context.instance, {}, Context::CONFIG};
    auto state = core.release();
    state.queueIndices.clear();

    Core const wrapped{context.instance, std::move(state)};

    ASSERT_EQ(wrapped.queueIndices().size(), wrapped.device().queues().size());
    for(size_t i = 0; i < wrapped.queueIndices().size(); i++)
        EXPECT_EQ(&wrapped.queue(i), &wrapped.device().queues()[i]);
}

CVK_TEST(Core, wrap_without_allocator_creates_one) {
    Context const context{};

    Core core{context.instance, {}, Context::CONFIG};
    auto state = core.release();
    state.allocator = nullptr;

    Core const wrapped{context.instance, std::move(state)};

    EXPECT_TRUE(wrapped.allocator().created());
    EXPECT_EQ(&wrapped.allocator().device(), &wrapped.device());
}

CVK_TEST(Core, queues_work_after_move) {
    Context const context{};

    Core core{context.instance, {}, Context::CONFIG};
    Core const moved{std::move(core)};

    EXPECT_FALSE(core.created());
    ASSERT_TRUE(moved.created());
    for(size_t i = 0; i < moved.queueIndices().size(); i++)
        moved.queue(i).waitIdle();
}

CVK_TEST(Core, present_queue_with_headless_surface) {
    test::PresentContext const context{};

    Core const core{context.instance, {context.surface()}, test::PresentContext::CONFIG};

    ASSERT_TRUE(core.created());
    EXPECT_TRUE(core.queue(0).canPresent());
    EXPECT_TRUE(context.surface().supportsFamily(core.physicalDevice(), core.queue(0).familyIndex()));
}

CVK_TEST(Core, move_assignment_destroys_the_old_core) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    test::InstanceContext const context{};

    Core core{context.instance, {}, Context::CONFIG};
    Core other{context.instance, {}, Context::CONFIG};
    auto const vkDevice = other.vkDevice();

    core = std::move(other);

    ASSERT_EQ(core.vkDevice(), vkDevice);
    EXPECT_FALSE(other.created());
    core.queue(0).waitIdle();
}

CVK_TEST(Core, objects_construct_before_the_core_is_created) {
    test::Context const context{};
    Core core{context.instance};

    DescriptorSetLayout::Config layoutConfig{};
    layoutConfig.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

    // the base constructors only store, none of them needs the created core
    Fence fence{core};
    Semaphore semaphore{core};
    TimelineSemaphore timeline{core};
    BaseBuffer buffer{core, {.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT}};
    Image image{
        core,
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        }
    };
    Sampler sampler{core, {}};
    CmdPool pool{core, test::pool_config(context.core)};
    PrimaryCmdBuffer primary{pool};
    DescriptorSetLayout layout{core, layoutConfig};
    PipelineLayout pipelineLayout{core, {.setLayouts = {layout}}};
    DescriptorPool descriptorPool{core, DescriptorPool::Config::ForLayouts({{layout}})};
    DescriptorSet set{descriptorPool, layout};

    core.create({}, Context::CONFIG);

    fence.create();
    semaphore.create();
    timeline.create(3);
    buffer.create(64);
    image.create({4, 4});
    sampler.create();
    pool.create();
    primary.create();
    layout.create();
    pipelineLayout.create();
    descriptorPool.create();
    set.create();

    EXPECT_TRUE(fence.created());
    EXPECT_TRUE(semaphore.created());
    EXPECT_EQ(timeline.gpuValue(), 3);
    EXPECT_TRUE(buffer.created());
    EXPECT_TRUE(image.created());
    EXPECT_TRUE(sampler.created());
    EXPECT_TRUE(primary.created());
    ASSERT_EQ(pipelineLayout.setLayouts().size(), 1);
    EXPECT_EQ(pipelineLayout.setLayouts()[0], layout.get());
    EXPECT_TRUE(set.created());
}
}
