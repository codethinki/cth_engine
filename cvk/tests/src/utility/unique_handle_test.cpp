#include "context.hpp"
#include "headless.hpp"
#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/debug/debug_messenger.hpp"
#include "cvk/data/img/sampler.hpp"
#include "cvk/render/pipeline/layout/descriptor_set_layout.hpp"
#include "cvk/render/pipeline/layout/pipeline_layout.hpp"
#include "cvk/render/pipeline/pipeline.hpp"
#include "cvk/render/pipeline/shader/shader.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/unique_handle.hpp"

#include <concepts>
#include <cstddef>


namespace cvk {
namespace {
    VkFence create_vk_fence(Core const& core) {
        VkFenceCreateInfo const info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
        VkFence vkFence = VK_NULL_HANDLE;
        [[maybe_unused]] auto const result = core.functions()->vkCreateFence(core.vkDevice(), &info, nullptr, &vkFence);
        return vkFence;
    }

    /**
     * @brief owner with a destroy that needs the pool, like a command buffer
     */
    struct PoolOwned {
        static void destroy(DeviceTable table, VkCommandPool vkPool, VkCommandBuffer vkBuffer) {
            table->vkFreeCommandBuffers(table.device(), vkPool, 1, &vkBuffer);
        }
    };

    /**
     * @brief custom deleter: derives from core_deleter, adds the pool
     */
    struct pool_deleter : core_deleter<PoolOwned> {
        pool_deleter(Core const& core, VkCommandPool vkPool) : core_deleter{core}, pool{vkPool} {}

        void operator()(VkCommandBuffer vkBuffer) const {
            PoolOwned::destroy(dev::device_table(core()), pool, vkBuffer);
        }

        VkCommandPool pool;
    };

    /**
     * @brief two handles destroyed together, like a VMA image with its allocation
     */
    struct pair_handle {
        int* first = nullptr;
        int* second = nullptr;

        pair_handle() = default;
        pair_handle(std::nullptr_t) {}
        pair_handle(int* first, int* second) : first{first}, second{second} {}

        // libstdc++ unique_ptr tests the pointer with if(ptr)
        explicit operator bool() const { return first != nullptr; }

        friend bool operator==(pair_handle const&, pair_handle const&) = default;
        friend bool operator==(pair_handle const& handle, std::nullptr_t) { return handle.first == nullptr; }
    };

    struct PairOwned {
        static inline int destroyed = 0;

        static void destroy(pair_handle handle) {
            delete handle.first;
            delete handle.second;
            ++destroyed;
        }
    };

    struct pair_deleter : handle_deleter<PairOwned> {
        using pointer = pair_handle;
    };
}

CVK_TEST(unique_handle, default_deleters_of_the_classes) {
    static_assert(std::same_as<default_deleter_t<Fence, VkFence>, core_deleter<Fence>>);
    static_assert(std::same_as<default_deleter_t<Semaphore, VkSemaphore>, core_deleter<Semaphore>>);
    static_assert(
        std::same_as<default_deleter_t<TimelineSemaphore, VkSemaphore>, core_deleter<TimelineSemaphore>>
    );
    static_assert(std::same_as<default_deleter_t<Shader, VkShaderModule>, core_deleter<Shader>>);
    static_assert(
        std::same_as<default_deleter_t<DescriptorSetLayout, VkDescriptorSetLayout>, core_deleter<DescriptorSetLayout>>
    );
    static_assert(std::same_as<default_deleter_t<PipelineLayout, VkPipelineLayout>, core_deleter<PipelineLayout>>);
    static_assert(std::same_as<default_deleter_t<Pipeline, VkPipeline>, core_deleter<Pipeline>>);
    static_assert(std::same_as<default_deleter_t<Sampler, VkSampler>, core_deleter<Sampler>>);

    static_assert(std::same_as<default_deleter_t<Surface, VkSurfaceKHR>, instance_deleter<Surface>>);
    static_assert(
        std::same_as<default_deleter_t<DebugMessenger, VkDebugUtilsMessengerEXT>, instance_deleter<DebugMessenger>>
    );

    static_assert(std::same_as<default_deleter_t<Instance, VkInstance>, handle_deleter<Instance>>);

    // destroy(VkDevice, PFN_vkDestroyDevice) -> needs a custom deleter
    static_assert(std::same_as<default_deleter_t<Device, VkDevice>, dev::no_default_deleter>);
}

CVK_TEST(unique_handle, size) {
    static_assert(sizeof(unique_handle<Instance, VkInstance>) == sizeof(VkInstance));
    static_assert(sizeof(unique_handle<Fence, VkFence>) == sizeof(Core const*) + sizeof(VkFence));
    static_assert(sizeof(unique_handle<Surface, VkSurfaceKHR>) == sizeof(Instance const*) + sizeof(VkSurfaceKHR));
}

CVK_TEST(unique_handle, core_deleter) {
    test::Context const context{};

    unique_handle<Fence, VkFence> handle{context.core};
    EXPECT_FALSE(handle);
    EXPECT_EQ(&handle.core(), &context.core);

    auto const vkFence = create_vk_fence(context.core);
    handle.reset(vkFence);
    ASSERT_TRUE(handle);
    EXPECT_EQ(handle.get(), vkFence);

    handle.reset();
    EXPECT_FALSE(handle);
}

CVK_TEST(unique_handle, move_assignment_destroys_the_old_handle) {
    test::Context const context{};
    unique_handle<Fence, VkFence> handle{context.core};
    unique_handle<Fence, VkFence> other{context.core};
    handle.reset(create_vk_fence(context.core));
    auto const vkFence = create_vk_fence(context.core);
    other.reset(vkFence);

    handle = std::move(other);

    EXPECT_EQ(handle.get(), vkFence);
    EXPECT_FALSE(other);
    EXPECT_EQ(&other.core(), &context.core);
}

CVK_TEST(unique_handle, release_keeps_the_handle) {
    test::Context const context{};
    unique_handle<Fence, VkFence> handle{context.core};
    auto const vkFence = create_vk_fence(context.core);
    handle.reset(vkFence);

    EXPECT_EQ(handle.release(), vkFence);
    EXPECT_FALSE(handle);

    Fence::destroy(context.core.deviceTable(), vkFence);
}

CVK_TEST(unique_handle, instance_deleter) {
    if constexpr(!constants::ENABLE_VALIDATION_LAYERS) GTEST_SKIP() << "needs the validation layers";

    test::reset_validation_errors();
    {
        Instance const instance{test::APP_NAME, test::headless_extensions(), test::counting_messenger()};
        auto surface = test::headless_surface(instance);

        unique_handle<Surface, VkSurfaceKHR> handle{instance};
        handle.reset(surface.release().vkSurface.get());

        EXPECT_EQ(&handle.instance(), &instance);
    }
    EXPECT_EQ(test::validation_errors(), 0);
}

CVK_TEST(unique_handle, handle_deleter) {
    VkApplicationInfo const appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = test::APP_NAME.data(),
        .applicationVersion = 0,
        .pEngineName = test::APP_NAME.data(),
        .engineVersion = 0,
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo const info{
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
    ASSERT_EQ(vkCreateInstance(&info, nullptr, &vkInstance), VK_SUCCESS);

    unique_handle<Instance, VkInstance> handle{};
    handle.reset(vkInstance);

    EXPECT_EQ(handle.get(), vkInstance);
}

CVK_TEST(unique_handle, custom_deleter) {
    test::Context const context{};
    auto const table = context.core.deviceTable();

    VkCommandPoolCreateInfo const poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = context.core.queue(0).familyIndex(),
    };
    VkCommandPool vkPool = VK_NULL_HANDLE;
    ASSERT_EQ(table->vkCreateCommandPool(table.device(), &poolInfo, nullptr, &vkPool), VK_SUCCESS);

    {
        unique_handle<PoolOwned, VkCommandBuffer, pool_deleter> handle{context.core, vkPool};
        EXPECT_EQ(&handle.core(), &context.core);
        EXPECT_EQ(handle.deleter().pool, vkPool);

        VkCommandBufferAllocateInfo const allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = vkPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer vkBuffer = VK_NULL_HANDLE;
        ASSERT_EQ(table->vkAllocateCommandBuffers(table.device(), &allocInfo, &vkBuffer), VK_SUCCESS);

        handle.reset(vkBuffer);
        EXPECT_EQ(handle.get(), vkBuffer);
    }

    table->vkDestroyCommandPool(table.device(), vkPool, nullptr);
}

CVK_TEST(unique_handle, non_pointer_handle) {
    PairOwned::destroyed = 0;
    {
        unique_handle<PairOwned, pair_handle, pair_deleter> handle{};
        EXPECT_FALSE(handle);

        handle.reset(pair_handle{new int{1}, new int{2}});
        ASSERT_TRUE(handle);
        EXPECT_EQ(*handle.get().second, 2);

        unique_handle<PairOwned, pair_handle, pair_deleter> other{};
        other.reset(pair_handle{new int{3}, new int{4}});

        handle = std::move(other);
        EXPECT_EQ(PairOwned::destroyed, 1);
        EXPECT_EQ(*handle.get().first, 3);
    }
    EXPECT_EQ(PairOwned::destroyed, 2);
}
}
