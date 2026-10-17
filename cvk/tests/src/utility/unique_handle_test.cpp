#include "context.hpp"
#include "headless.hpp"
#include "test.hpp"
#include "validation.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/timeline_semaphore.hpp"
#include "cvk/surface/surface.hpp"
#include "cvk/utility/constants.hpp"
#include "cvk/utility/core_handle.hpp"
#include "cvk/utility/instance_handle.hpp"
#include "cvk/utility/unique_handle.hpp"
#include "cvk/utility/vma_handle.hpp"

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
        using vk_handle_t = VkCommandBuffer;

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
            PoolOwned::destroy(core().deviceTable(), pool, vkBuffer);
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
        using vk_handle_t = pair_handle;

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

    /**
     * @brief owner of heap memory, constant evaluation rejects leaks and double frees
     */
    struct HeapOwned {
        using vk_handle_t = int*;

        static constexpr void destroy(int* ptr) { delete ptr; }
    };

    using heap_handle = unique_handle<int*, handle_deleter<HeapOwned>>;

    consteval int move_assigned_value() {
        heap_handle handle{};
        handle.reset(new int{1});
        heap_handle other{new int{2}};

        handle = std::move(other);
        return *handle.get();
    }

    consteval int released_value() {
        heap_handle handle{new int{3}};
        int* const ptr = handle.release();

        int const value = *ptr;
        delete ptr;
        return value;
    }

    /**
     * @brief reset(handle, context...) compiles
     */
    template<class H, class... Context>
    concept resettable = requires(H& handle, typename H::handle_t vkHandle, Context&&... context) {
        handle.reset(vkHandle, std::forward<Context>(context)...);
    };

    template<class H>
    concept deleter_assignable = requires(H& handle, typename H::deleter_t deleter) { handle.deleter() = deleter; };
}

namespace {
    /**
     * @brief a class without vk_handle_t
     */
    struct Unnamed {
        static void destroy(DeviceTable, VkFence) {}
    };

    template<class T>
    concept core_deleter_of = requires { typename core_deleter<T>; };
}

CVK_TEST(unique_handle, owner_names_its_handle) {
    static_assert(vk_handle_owner<Fence>);
    static_assert(vk_handle_owner<TimelineSemaphore>);
    static_assert(std::same_as<Fence::vk_handle_t, VkFence>);
    static_assert(std::same_as<core_handle<Fence>::handle_t, VkFence>);

    static_assert(!vk_handle_owner<Unnamed>);
    static_assert(!core_deleter_of<Unnamed>);
}

CVK_TEST(unique_handle, a_handle_needs_its_context) {
    using fence_handle = core_handle<Fence>;
    static_assert(!std::default_initializable<fence_handle>);
    static_assert(!std::constructible_from<fence_handle, VkFence>);
    static_assert(!resettable<fence_handle>);
    static_assert(resettable<fence_handle, Core const&>);

    using surface_handle = instance_handle<Surface>;
    static_assert(!resettable<surface_handle>);
    static_assert(resettable<surface_handle, VkInstance>);

    using instance_handle = unique_handle<VkInstance, handle_deleter<Instance>>;
    static_assert(std::default_initializable<instance_handle>);
    static_assert(resettable<instance_handle>);

    using memory_handle = vma_handle<Memory>;
    static_assert(!resettable<memory_handle>);
    static_assert(resettable<memory_handle, Core const&>);

    using pool_handle = unique_handle<VkCommandBuffer, pool_deleter>;
    static_assert(!resettable<pool_handle, Core const&>);
    static_assert(resettable<pool_handle, Core const&, VkCommandPool>);

    // the context of a handle cannot change on its own
    static_assert(!deleter_assignable<fence_handle>);
}

CVK_TEST(unique_handle, constexpr) {
    static_assert(move_assigned_value() == 2);
    static_assert(released_value() == 3);
}

CVK_TEST(unique_handle, size) {
    static_assert(sizeof(unique_handle<VkInstance, handle_deleter<Instance>>) == sizeof(VkInstance));
    static_assert(sizeof(core_handle<Fence>) == sizeof(Core const*) + sizeof(VkFence));
    static_assert(sizeof(vma_handle<Memory>) == sizeof(Core const*) + sizeof(VmaAllocation));
    static_assert(
        sizeof(instance_handle<Surface>) == sizeof(VkInstance) + sizeof(VkSurfaceKHR)
    );
}

CVK_TEST(unique_handle, core_deleter) {
    test::Context const context{};

    core_handle<Fence> handle{context.core};
    EXPECT_FALSE(handle);
    EXPECT_EQ(&handle.core(), &context.core);

    auto const vkFence = create_vk_fence(context.core);
    handle.reset(vkFence, context.core);
    ASSERT_TRUE(handle);
    EXPECT_EQ(handle.get(), vkFence);

    handle.reset();
    EXPECT_FALSE(handle);
}

CVK_TEST(unique_handle, move_assignment_destroys_the_old_handle) {
    test::Context const context{};
    core_handle<Fence> handle{context.core};
    core_handle<Fence> other{context.core};
    handle.reset(create_vk_fence(context.core), context.core);
    auto const vkFence = create_vk_fence(context.core);
    other.reset(vkFence, context.core);

    handle = std::move(other);

    EXPECT_EQ(handle.get(), vkFence);
    EXPECT_FALSE(other);
    EXPECT_EQ(&other.core(), &context.core);
}

CVK_TEST(unique_handle, release_keeps_the_handle) {
    test::Context const context{};
    auto const vkFence = create_vk_fence(context.core);
    core_handle<Fence> handle{vkFence, context.core};

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

        instance_handle<Surface> handle{surface.release().vkSurface.get(), instance.get()};

        EXPECT_EQ(handle.vkInstance(), instance.get());
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
    // Instance::destroy() calls vkDestroyInstance through volk
    volkLoadInstanceOnly(vkInstance);

    unique_handle<VkInstance, handle_deleter<Instance>> handle{};
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
        unique_handle<VkCommandBuffer, pool_deleter> handle{context.core, vkPool};
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

        handle.reset(vkBuffer, context.core, vkPool);
        EXPECT_EQ(handle.get(), vkBuffer);
    }

    table->vkDestroyCommandPool(table.device(), vkPool, nullptr);
}

CVK_TEST(unique_handle, non_pointer_handle) {
    PairOwned::destroyed = 0;
    {
        unique_handle<pair_handle, pair_deleter> handle{};
        EXPECT_FALSE(handle);

        handle.reset(pair_handle{new int{1}, new int{2}});
        ASSERT_TRUE(handle);
        EXPECT_EQ(*handle.get().second, 2);

        unique_handle<pair_handle, pair_deleter> other{};
        other.reset(pair_handle{new int{3}, new int{4}});

        handle = std::move(other);
        EXPECT_EQ(PairOwned::destroyed, 1);
        EXPECT_EQ(*handle.get().first, 3);
    }
    EXPECT_EQ(PairOwned::destroyed, 2);
}
}
