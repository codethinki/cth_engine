#pragma once

#include "cth/numeric.hpp"

#include "src/interface/render/RenderPulse.hpp"
#include "src/vulkan/base/queue/CthPresentInfo.hpp"
#include "src/vulkan/base/queue/CthQueue.hpp"
#include "src/vulkan/resource/image/CthImage.hpp"


#include <volk.h>


#include <memory>
#include <vector>



namespace cth::vk {
struct PresentInfo;
class Fence;
class Subpass;
class AttachmentCollection;
class Image;
class ImageView;
class CmdBuffer;
class PrimaryCmdBuffer;
class Surface;
class GraphicsSyncConfig;
class Queue;
class Core;

//TODO add release and wrap

class Swapchain {
public:
    Swapchain(Core const& core, Queue const& present_queue, GraphicsSyncConfig const& sync_config, Surface const& surface);

    Swapchain(Core const& core, Queue const& present_queue, GraphicsSyncConfig const& sync_config,
        Surface const& surface, create_t);
    virtual ~Swapchain();

    //IMPLEMENT virtual void wrap(const Surface* surface, VkExtent2D window_extent);
    virtual void create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);

    /**
     * @brief destroys the swapchain
     * @note requires @ref created()
     */
    virtual void destroy();


    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }


    virtual void resize(VkExtent2D window_extent);



    /**
     * @return result of @ref vkAcquireNextImageKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR]
     *
     * @note might block
     * @note the semaphore must not be signaled
     * @note the fence must be signaled
     */
    VkResult acquireNextImage();
    void skipAcquire() const;


    [[nodiscard]] VkResult present();
    void skipPresent();

    void changeSwapchainImageQueue(uint32_t release_queue, CmdBuffer const& release_cmd_buffer, uint32_t acquire_queue,
        CmdBuffer const& acquire_cmd_buffer, uint32_t image_index) const;

    /**
     * @brief adds a minimum of required flags for the resolve subpass
     */
    static void addResolveSubpassDependencyFlags(VkSubpassDependency& swap_subpass_dependency);

    static void destroy(DeviceTable table, VkSwapchainKHR swapchain);

private:
    static constexpr uint32_t NO_IMAGE_INDEX = (std::numeric_limits<uint32_t>::max());

    void initSyncObjects();
    void initAttachments();
    void init();

    void setImageFormat(VkFormat format);

    //setMsaaSampleCount
    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;


    //createSyncObjects
    void createSyncObjects();

    [[nodiscard]] static VkExtent2D chooseSwapExtent(VkExtent2D window_extent, VkSurfaceCapabilitiesKHR const& capabilities);
    [[nodiscard]] static uint32_t evalMinImageCount(uint32_t min, uint32_t max);
    [[nodiscard]] static VkSwapchainCreateInfoKHR createInfo(VkSurfaceKHR surface,
        VkSurfaceFormatKHR surface_format, VkSurfaceCapabilitiesKHR const& capabilities, VkPresentModeKHR present_mode, VkExtent2D extent,
        uint32_t image_count, VkSwapchainKHR old_swapchain);

    void createSwapchain(VkExtent2D window_extent, VkSwapchainKHR old_swapchain);


    [[nodiscard]] Image::Config createColorImageConfig(VkSampleCountFlagBits samples) const;

    [[nodiscard]] std::vector<std::unique_ptr<Image>> getSwapchainImages();

    void createResolveAttachments();


    void createPresentInfos();


    void destroyResources();

    void destroySwapchain(VkSwapchainKHR swapchain) const;

    void destroySyncObjects();
    //TEMP left off here check swapchain destruction and then try to make it compile

    void resizeReset();
    void reset();

    cth::not_null<Core const*> _core;
    cth::not_null<Queue const*> _presentQueue;
    cth::not_null<Surface const*> _surface;


    cth::move_ptr<VkSwapchainKHR_T> _handle = VK_NULL_HANDLE;


    VkExtent2D _extent{};
    float _aspectRatio = 0;
    VkExtent2D _windowExtent{};


    size_t _imageCount = 0;
    VkFormat _imageFormat = VK_FORMAT_UNDEFINED;

    std::unique_ptr<AttachmentCollection> _resolveAttachments;

    cth::not_null<GraphicsSyncConfig const*> _syncConfig;

    std::vector<Fence> _imageAvailableFences;

    std::vector<PresentInfo> _presentInfos;


    std::array<uint32_t, constants::FRAMES_IN_FLIGHT> _imageIndices{};

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    [[nodiscard]] Core const& core() const { return *_core; }

public:
    [[nodiscard]] VkSwapchainKHR get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] float extentAspectRatio() const { return _aspectRatio; }
    [[nodiscard]] auto imageIndex(size_t pulse_value) const {
        CTH_CRITICAL(!cth::num::in(pulse_value, 0, _imageIndices.size()), "pulse value out of bounds") {}
        return _imageIndices[pulse_value];
    }
    [[nodiscard]] auto imageIndex(RenderPulse const& pulse) const { return imageIndex(pulse.get()); }

    [[nodiscard]] size_t size() const { return _imageCount; }
    [[nodiscard]] ImageConfig imageConfig() const;
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; } //TODO move this to framebuffer or render pass
    [[nodiscard]] AttachmentCollection const* resolveAttachments() const { return _resolveAttachments.get(); }
    [[nodiscard]] auto extent() const { return _extent; }

    Swapchain(Swapchain const& other) = delete;
    Swapchain(Swapchain&& other) noexcept = default;
    Swapchain& operator=(Swapchain const& other) = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = default;

    static void debug_check(Swapchain const& swapchain);
    static void debug_check_leak(Swapchain const* swapchain);

    static void debug_check_window_extent(VkExtent2D window_extent);
    static void debug_check_compatibility(Swapchain const& a, Swapchain const& b);
};

}

//debug checks

namespace cth::vk {

inline void Swapchain::debug_check(Swapchain const& swapchain) {
    CTH_CRITICAL(swapchain._handle == VK_NULL_HANDLE, "swapchain handle invalid (VK_NULL_HANDLE)") {}
    CTH_CRITICAL(swapchain.size() == 0, "swapchain size must not be 0") {}
}
inline void Swapchain::debug_check_leak(Swapchain const* swapchain) {
    CTH_WARN(swapchain->_handle != VK_NULL_HANDLE, "swapchain handle replaced, (potential memory leak)") {}
}
inline void Swapchain::debug_check_window_extent(VkExtent2D window_extent) {
    CTH_CRITICAL(
        window_extent.width == 0 || window_extent.height == 0,
        "window_extent width({0}) or height({0}) invalid (> 0 required)",
        window_extent.width, window_extent.height
    ) {}
}
inline void Swapchain::debug_check_compatibility(Swapchain const& a, Swapchain const& b) {
    CTH_CRITICAL(a._core == b._core, "swapchains not compatible (different cores)") {}
}

}
