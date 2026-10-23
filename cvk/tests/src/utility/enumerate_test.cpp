#include "test.hpp"

#include "cvk/utility/enumerate.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <volk.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <span>


namespace cvk {
namespace {
    constexpr std::array ELEMENTS{1, 2, 3, 4};

    /**
     * @brief acts like a Vulkan query with the available elements
     * @return VK_INCOMPLETE -> data has less space than available
     */
    VkResult query(std::span<int const> available, uint32_t* count, int* data) {
        if(data == nullptr) {
            *count = static_cast<uint32_t>(available.size());
            return VK_SUCCESS;
        }

        auto const written = std::min<size_t>(*count, available.size());
        std::ranges::copy(available.first(written), data);
        *count = static_cast<uint32_t>(written);
        return written < available.size() ? VK_INCOMPLETE : VK_SUCCESS;
    }

    /**
     * @brief 2 elements at the first call, 4 at the later calls -> the first write is incomplete
     */
    VkResult growing(uint32_t* calls, uint32_t* count, int* data) {
        auto const call = (*calls)++;
        return query(std::span{ELEMENTS}.first(call == 0 ? 2 : 4), count, data);
    }

    /**
     * @brief 4 elements at the count call, 2 at the write call
     */
    VkResult shrinking(uint32_t* calls, uint32_t* count, int* data) {
        auto const call = (*calls)++;
        return query(std::span{ELEMENTS}.first(call == 0 ? 4 : 2), count, data);
    }

    /**
     * @brief one element at every count call, VK_INCOMPLETE at every write call
     */
    VkResult always_incomplete(uint32_t* calls, uint32_t* count, int* data) {
        ++*calls;
        if(data != nullptr) return VK_INCOMPLETE;

        *count = 1;
        return VK_SUCCESS;
    }

    /**
     * @return VK_ERROR_OUT_OF_HOST_MEMORY at the call with the index failingCall
     */
    VkResult failing(uint32_t failingCall, uint32_t* calls, uint32_t* count, int* data) {
        if((*calls)++ == failingCall) return VK_ERROR_OUT_OF_HOST_MEMORY;
        return query(ELEMENTS, count, data);
    }

    /**
     * @brief writes the first elementCount elements, like vkGetPhysicalDeviceQueueFamilyProperties()
     */
    void void_query(uint32_t elementCount, uint32_t* count, int* data) {
        [[maybe_unused]] auto const result = query(std::span{ELEMENTS}.first(elementCount), count, data);
    }
}

CVK_TEST(enumerate, element_type_is_the_pointee_of_the_last_parameter) {
    static_assert(std::same_as<dev::enumerate_element_t<decltype(&void_query)>, int>);
    static_assert(std::same_as<dev::enumerate_element_t<PFN_vkEnumeratePhysicalDevices>, VkPhysicalDevice>);
    static_assert(std::same_as<dev::enumerate_element_t<PFN_vkGetSwapchainImagesKHR>, VkImage>);
}

CVK_TEST(enumerate, incomplete_repeats_until_complete) {
    uint32_t calls = 0;

    auto const elements = dev::enumerate("fake elements", growing, &calls);

    EXPECT_EQ(calls, 4);
    EXPECT_TRUE(std::ranges::equal(elements, ELEMENTS));
}

CVK_TEST(enumerate, incomplete_in_every_attempt_throws) {
    uint32_t calls = 0;

    try {
        [[maybe_unused]] auto const elements = dev::enumerate("fake elements", always_incomplete, &calls);
        ADD_FAILURE() << "no throw";
    } catch(vk_result_exception const& exception) {
        EXPECT_EQ(exception.vkResult(), VK_INCOMPLETE);
    }
    EXPECT_EQ(calls, 2 * dev::MAX_QUERY_ATTEMPTS);
}

CVK_TEST(enumerate, fewer_written_elements_shrink_the_result) {
    uint32_t calls = 0;

    auto const elements = dev::enumerate("fake elements", shrinking, &calls);

    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(std::ranges::equal(elements, std::span{ELEMENTS}.first(2)));
}

CVK_TEST(enumerate, error_throws_the_result) {
    // count call, write call
    for(uint32_t const failingCall : {0u, 1u}) {
        uint32_t calls = 0;
        try {
            [[maybe_unused]] auto const elements = dev::enumerate("fake elements", failing, failingCall, &calls);
            ADD_FAILURE() << "no throw at call " << failingCall;
        } catch(vk_result_exception const& exception) {
            EXPECT_EQ(exception.vkResult(), VK_ERROR_OUT_OF_HOST_MEMORY);
        }
        EXPECT_EQ(calls, failingCall + 1);
    }
}

CVK_TEST(enumerate, void_function_returns_its_elements) {
    auto const elements = dev::enumerate("fake elements", void_query, 3u);

    EXPECT_TRUE(std::ranges::equal(elements, std::span{ELEMENTS}.first(3)));
}
}
