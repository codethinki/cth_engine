#include "vk/utility/os.hpp"

#include "os/incl_windows.hpp"

#include "vk/base/CthInstance.hpp"
#include "vk/surface/CthSurface.hpp"
#include "vk/utility/cth_vk_exceptions.hpp"

namespace cth::vk::os {

std::unique_ptr<Surface> TempSurface(Instance const& instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // Create a hidden window for the surface
    std::wstring const name = L"TempHiddenWindow";
    WNDCLASSEX const wc{
        .cbSize = sizeof(wc),
        .lpfnWndProc = DefWindowProc,
        .hInstance = GetModuleHandle(nullptr),
        .lpszClassName = name.data(),
    };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(0, name.data(), L"TempHiddenSurface", 0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    CTH_STABLE_ERR(hwnd == nullptr, "failed to create temp window")
        throw details->exception();


    // Create the Vulkan surface
    VkWin32SurfaceCreateInfoKHR const createInfo{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = GetModuleHandle(nullptr),
        .hwnd = hwnd
    };


    auto const result = vkCreateWin32SurfaceKHR(instance.get(), &createInfo, nullptr, &surface);
    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create temp surface") {
        DestroyWindow(hwnd);
        throw vk::result_exception{result, details->exception()};
    }

    log::msg("created temp surface");

    return std::make_unique<Surface>(instance, nullptr, Surface::Config{}, Surface::State{surface});
}
}
