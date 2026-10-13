#include <gtest/gtest.h>
#include <volk.h>

#include <vector>


namespace {

TEST(loader, supports_vulkan_1_3) {
    ASSERT_EQ(volkInitialize(), VK_SUCCESS);

    uint32_t version = 0;
    ASSERT_EQ(vkEnumerateInstanceVersion(&version), VK_SUCCESS);
    EXPECT_GE(version, VK_API_VERSION_1_3);

    volkFinalize();
}

TEST(loader, finds_vulkan_1_3_device) {
    ASSERT_EQ(volkInitialize(), VK_SUCCESS);

    VkApplicationInfo const appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo const createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
    };

    VkInstance instance = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateInstance(&createInfo, nullptr, &instance), VK_SUCCESS);
    volkLoadInstanceOnly(instance);

    uint32_t deviceCount = 0;
    ASSERT_EQ(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), VK_SUCCESS);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    ASSERT_EQ(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), VK_SUCCESS);

    bool found = false;
    for(auto const device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        found |= properties.apiVersion >= VK_API_VERSION_1_3;
    }
    EXPECT_TRUE(found);

    vkDestroyInstance(instance, nullptr);
    volkFinalize();
}

}
