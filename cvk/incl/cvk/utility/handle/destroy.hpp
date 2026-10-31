#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <span>
#include <type_traits>

/**
 * @file
 * the global destroy of the raw handles, one overload per handle kind
 * @details the definitions are in the .cpp of the object,
 *  the deleters are templates: a qualified name in a template is looked up at its definition,
 *  every overload must be declared before the deleters -> all overloads are declared here
 */


namespace cvk {
static_assert(
    std::is_pointer_v<VkSemaphore>,
    "cvk::destroy() needs distinct 64 bit pointer handles (VK_USE_64_BIT_PTR_DEFINES == 1), got integer handles"
);

// outside the core
/**
 * @pre volk loaded the instance functions, @ref Instance::create() and @ref Instance::wrap() load them
 */
void destroy(VkInstance);
void destroy(vk_not_null<VkInstance>, VkDebugUtilsMessengerEXT);
void destroy(vk_not_null<VkInstance>, VkSurfaceKHR);

/**
 * destroys the device, gets vkDestroyDevice through vkGetDeviceProcAddr()
 */
void destroy(VkDevice);
void destroy(VmaAllocator);

// memory
void destroy(VmaAllocator, VmaAllocation);

// device objects
void destroy(DeviceTable, VkSemaphore);
void destroy(DeviceTable, VkFence);
void destroy(DeviceTable, VkSwapchainKHR);
void destroy(DeviceTable, VkImage);
void destroy(DeviceTable, VkImageView);
void destroy(DeviceTable, VkSampler);
void destroy(DeviceTable, VkBuffer);
void destroy(DeviceTable, VkDescriptorPool);
void destroy(DeviceTable, VkDescriptorSetLayout);
void destroy(DeviceTable, VkPipelineLayout);
void destroy(DeviceTable, VkShaderModule);
void destroy(DeviceTable, VkPipeline);
void destroy(DeviceTable, VkCommandPool);

// pool children
void destroy(DeviceTable, VkCommandPool, VkCommandBuffer);

/**
 * frees the command buffers at once
 */
void destroy(DeviceTable, VkCommandPool, std::span<VkCommandBuffer const> vkBuffers);

void destroy(DeviceTable, VkDescriptorPool, VkDescriptorSet);
}
