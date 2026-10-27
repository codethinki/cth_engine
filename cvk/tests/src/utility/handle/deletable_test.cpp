#include "test.hpp"

#include "cvk/base/core.hpp"
#include "cvk/base/device_table.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/memory/memory.hpp"
#include "cvk/debug/debug_messenger.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/utility/handle/deletable.hpp"

#include <concepts>
#include <utility>


namespace cvk {
namespace {
    /**
     * owner whose destroy writes down its context and its handle
     */
    struct Recorded {
        using vk_handle_t = int*;

        static inline int context = 0;
        static inline int* handle = nullptr;

        static void destroy(int destroyContext, int* ptr) {
            context = destroyContext;
            handle = ptr;
        }
    };

    /**
     * owner without context that counts its destroys
     */
    struct Counted {
        using vk_handle_t = int*;

        static inline int destroyed = 0;

        static void destroy(int*) { ++destroyed; }
    };
}

CVK_TEST(deletable, the_state_names_its_deletable) {
    static_assert(std::same_as<deletable_t<Semaphore>, device_deletable<Semaphore>>);
    static_assert(std::same_as<deletable_t<Memory>, vma_deletable<Memory>>);
    static_assert(std::same_as<deletable_t<Image>, Image::Deletable>);
    static_assert(std::same_as<deletable_t<Instance>, Instance::Deletable>);

    static_assert(destroyable<Semaphore>);
    static_assert(destroyable<Memory>);
    static_assert(destroyable<Image>);
    static_assert(destroyable<Instance>);
    static_assert(!destroyable<Core>);
}

CVK_TEST(deletable, size) {
    static_assert(sizeof(device_deletable<Semaphore>) == sizeof(DeviceTable) + sizeof(VkSemaphore));
    static_assert(sizeof(device_deletable<Semaphore>) == 3 * sizeof(void*));
    static_assert(sizeof(vma_deletable<Memory>) == sizeof(VmaAllocator) + sizeof(VmaAllocation));
    static_assert(
        sizeof(instance_deletable<DebugMessenger>) == sizeof(VkInstance) + sizeof(VkDebugUtilsMessengerEXT)
    );
}

CVK_TEST(deletable, call_passes_the_context_and_the_handle) {
    int value = 0;

    basic_deletable<Recorded, int>{{7}, &value}();

    EXPECT_EQ(Recorded::context, 7);
    EXPECT_EQ(Recorded::handle, &value);
}

CVK_TEST(deletable, only_the_call_destroys) {
    int value = 0;
    Counted::destroyed = 0;

    {
        [[maybe_unused]] basic_deletable<Counted> const deletable{{}, &value};
    }
    EXPECT_EQ(Counted::destroyed, 0);

    basic_deletable<Counted> deletable{{}, &value};
    std::move(deletable)();
    EXPECT_EQ(Counted::destroyed, 1);
}
}
