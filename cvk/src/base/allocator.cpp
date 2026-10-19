#include "cvk/base/allocator.hpp"

#include "cvk/base/device.hpp"
#include "cvk/base/instance.hpp"
#include "cvk/base/physical_device.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>
#include <string_view>


namespace cvk {
Allocator::Allocator(Device const& device) : _device{&device} { Device::debug_check(device); }

Allocator::Allocator(Device const& device, State const& state) : Allocator{device} { wrap(state); }

Allocator::Allocator(Device const& device, create_t) : Allocator{device} { create(); }

void Allocator::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vmaAllocator.get());
}

void Allocator::create() {
    optDestroy();

    VmaAllocatorCreateInfo info{
        .flags = flags(*_device),
        .physicalDevice = _device->physicalDevice().get(),
        .device = _device->get(),
        .preferredLargeHeapBlockSize = 0,
        .pAllocationCallbacks = nullptr,
        .pDeviceMemoryCallbacks = nullptr,
        .pHeapSizeLimit = nullptr,
        .pVulkanFunctions = nullptr,
        .instance = _device->instance().get(),
        .vulkanApiVersion = VK_API_VERSION_1_3,
        .pTypeExternalMemoryHandleTypes = nullptr,
    };

    VmaVulkanFunctions functions{};
    auto const importResult = vmaImportVulkanFunctionsFromVolk(&info, &functions);
    CTH_STABLE_ERR(importResult != VK_SUCCESS, "failed to import the vulkan functions for VMA") {
        forget();
        throw cvk::vk_result_exception{importResult, details->exception()};
    }
    info.pVulkanFunctions = &functions;

    VmaAllocator ptr = VK_NULL_HANDLE;
    auto const result = vmaCreateAllocator(&info, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create allocator") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr);
}

void Allocator::destroy() {
    debug_check(*this);

    _handle.reset();

    forget();
}

Allocator::State Allocator::release() {
    debug_check(*this);

    State const state{_handle.release()};
    forget();
    return state;
}

void Allocator::destroy(VmaAllocator vmaAllocator) {
    CTH_WARN(vmaAllocator == VK_NULL_HANDLE, "vmaAllocator invalid (VK_NULL_HANDLE)") {}

    vmaDestroyAllocator(vmaAllocator);
}

VmaAllocatorCreateFlags Allocator::flags(Device const& device) {
    auto const& features = device.features();

    VmaAllocatorCreateFlags flags = 0;
    if(features.bufferDeviceAddress) flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    if(features.maintenance4) flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
    if(std::ranges::contains(device.extensions(), std::string_view{VK_EXT_MEMORY_BUDGET_EXTENSION_NAME}))
        flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    return flags;
}

void Allocator::forget() {}
}
