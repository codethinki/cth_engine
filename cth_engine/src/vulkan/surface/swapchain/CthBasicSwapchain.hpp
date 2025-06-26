#pragma once

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
class RenderPass;
class AttachmentCollection;
class Image;
class Framebuffer;
class ImageView;
class CmdBuffer;
class PrimaryCmdBuffer;
class Surface;
class GraphicsSyncConfig;
class Queue;
class Core;

//TEMP make this non basic and remove components like renderpass and subpass as well as attachments (except resolve attachment)
//TEMP maybe remove framebuffers idk

class BasicSwapchain {
public:
    BasicSwapchain(Core const& core, Queue const& present_queue, GraphicsSyncConfig const& sync_config,
        Surface const& surface);
    virtual ~BasicSwapchain();

    //IMPLEMENT virtual void wrap(const Surface* surface, VkExtent2D window_extent);
    virtual void create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);

    /**
     * @brief destroys the swapchain
     * @note calls destroyResources()
     */
    virtual void destroy();


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

    void beginRenderPass(PrimaryCmdBuffer const& cmd_buffer) const;

    void endRenderPass(PrimaryCmdBuffer const& cmd_buffer) const;


    [[nodiscard]] VkResult present();
    void skipPresent();

    void changeSwapchainImageQueue(uint32_t release_queue, CmdBuffer const& release_cmd_buffer, uint32_t acquire_queue,
        CmdBuffer const& acquire_cmd_buffer, uint32_t image_index) const;

    [[nodiscard]] ImageView const& imageView(size_t index) const;
    [[nodiscard]] Image const* image(size_t index) const;

    /**
     * @brief adds a minimum of required flags for the resolve subpass
     */
    static void addResolveSubpassDependencyFlags(VkSubpassDependency& swap_subpass_dependency);

    static void destroy(DeviceTable table, VkSwapchainKHR swapchain);

private:
    static constexpr uint32_t NO_IMAGE_INDEX = (std::numeric_limits<uint32_t>::max());

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
    [[nodiscard]] Image::Config createDepthImageConfig() const;

    [[nodiscard]] std::vector<std::unique_ptr<Image>> getSwapchainImages();
    void findDepthFormat();


    void createResolveAttachments(std::vector<std::unique_ptr<Image>> swapchain_images);
    void createMsaaAttachments();
    void createDepthAttachments();

    void createAttachments();


    //createRenderPass
    void createSubpass();
    [[nodiscard]] VkSubpassDependency createSubpassDependency() const;
    /**
     * @throws cth::vk::result_exception result of @ref vkCreateRenderPass()
     */
    void createRenderPass();
    /**
     * @throws cth::vk::result_exception result of @ref vkCreateFramebuffer()
     */
    void createFramebuffers();

    void createPresentInfos();


    void destroyRenderConstructs();
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
    VkFormat _depthFormat = VK_FORMAT_UNDEFINED;

    std::unique_ptr<AttachmentCollection> _resolveAttachments;
    std::unique_ptr<AttachmentCollection> _msaaAttachments; //TEMP this should not be here
    std::unique_ptr<AttachmentCollection> _depthAttachments; //TEMP this should not be here

    std::unique_ptr<RenderPass> _renderPass; //TEMP this should not be here
    std::unique_ptr<Subpass> _subpass; //TEMP this should not be here
    std::vector<Framebuffer> _swapchainFramebuffers; //TEMP this maybe should not be here


    cth::not_null<GraphicsSyncConfig const*> _syncConfig;

    std::vector<Fence> _imageAvailableFences;

    std::vector<PresentInfo> _presentInfos;


    std::array<uint32_t, constants::FRAMES_IN_FLIGHT> _imageIndices{};

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    [[nodiscard]] Core const& core() const { return *_core; }

public:
    [[nodiscard]] VkSwapchainKHR get() const { return _handle.get(); }
    [[nodiscard]] float extentAspectRatio() const { return _aspectRatio; }
    [[nodiscard]] RenderPass const* renderPass() const { return _renderPass.get(); }
    [[nodiscard]] auto imageIndex(size_t pulse_value) const { return _imageIndices[pulse_value]; }
    [[nodiscard]] auto imageIndex(RenderPulse const& pulse) const { return imageIndex(pulse.get()); }

    [[nodiscard]] size_t size() const { return _imageCount; }
    [[nodiscard]] ImageConfig imageConfig() const;
    [[nodiscard]] VkFormat imageFormat() const { return _imageFormat; }
    [[nodiscard]] VkSampleCountFlagBits msaaSamples() const { return _msaaSamples; } //TODO move this to framebuffer or render pass
    [[nodiscard]] AttachmentCollection const* resolveAttachments() const { return _resolveAttachments.get(); }
    [[nodiscard]] auto extent() const { return _extent; }

    BasicSwapchain(BasicSwapchain const& other) = delete;
    BasicSwapchain(BasicSwapchain&& other) noexcept = default;
    BasicSwapchain& operator=(BasicSwapchain const& other) = delete;
    BasicSwapchain& operator=(BasicSwapchain&& other) noexcept = default;

    static void debug_check(BasicSwapchain const* swapchain);
    static void debug_check_leak(BasicSwapchain const* swapchain);

    static void debug_check_window_extent(VkExtent2D window_extent);
    static void debug_check_compatibility(BasicSwapchain const& a, BasicSwapchain const& b);
};

}

//debug checks

namespace cth::vk {

inline void BasicSwapchain::debug_check(BasicSwapchain const* swapchain) {
    CTH_CRITICAL(swapchain == nullptr, "swapchain invalid (nullptr)") {}
    CTH_CRITICAL(swapchain->_handle == VK_NULL_HANDLE, "swapchain handle invalid (VK_NULL_HANDLE)") {}
}
inline void BasicSwapchain::debug_check_leak(BasicSwapchain const* swapchain) {
    CTH_WARN(swapchain->_handle != VK_NULL_HANDLE, "swapchain handle replaced, (potential memory leak)") {}
}
inline void BasicSwapchain::debug_check_window_extent(VkExtent2D window_extent) {
    CTH_CRITICAL(window_extent.width == 0 || window_extent.height == 0, "window_extent width({0}) or height({0}) invalid (> 0 required",
        window_extent.width, window_extent.height) {}
}
inline void BasicSwapchain::debug_check_compatibility(BasicSwapchain const& a, BasicSwapchain const& b) {
    CTH_CRITICAL(a._core == b._core, "swapchains not compatible (different cores)") {}
}

}
