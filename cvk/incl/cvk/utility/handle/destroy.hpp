#pragma once
#include "cvk/base/device_table.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>
#include <vk_mem_alloc.h>

#include <span>

/**
 * @file
 * the global destroy of the raw handles, one overload per handle kind
 * @details declares every overload: the deleters look them up at their definition
 * @details definitions: in the .cpp of the object
 */


namespace cvk {
// outside the core
/**
 * @pre volk loaded the instance functions, `Instance::create()` and `Instance::wrap()` load them
 */
void destroy(VkInstance);
void destroy(vk_not_null<VkInstance>, VkDebugUtilsMessengerEXT);
void destroy(vk_not_null<VkInstance>, VkSurfaceKHR);

/**
 * destroys the device
 * @details gets `vkDestroyDevice` through `vkGetDeviceProcAddr()`
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
void destroy(DeviceTable, VkBuffer);
void destroy(DeviceTable, VkShaderModule);
void destroy(DeviceTable, VkPipeline);
void destroy(DeviceTable, VkCommandPool);

// pooled handles
void destroy(DeviceTable, VkCommandPool, VkCommandBuffer);

/**
 * frees the command buffers at once
 */
void destroy(DeviceTable, VkCommandPool, std::span<VkCommandBuffer const> vkBuffers);
}
