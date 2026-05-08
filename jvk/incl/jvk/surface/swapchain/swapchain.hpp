#pragma once
#include "jvk/base/device_table.hpp"
#include "jvk/render/sync/semaphore.hpp"
#include "jvk/res/img/image_config.hpp"
#include "jvk/surface/swapchain/swapchain_config.hpp"
#include "jvk/utility/constants.hpp"



#include <volk.h>
#include <cth/numeric.hpp>

#include <memory>
#include <vector>



namespace jvk {
struct PresentInfo;
class Fence;
class Subpass;
class AttachmentCollection;
class Image;
class CmdBuffer;
class Surface;
class GraphicsSyncConfig;
class Queue;
class Core;

//TODO add release and wrap

class Swapchain {
public:
    using Config = SwapchainConfig;

    Swapchain(
        Core const& core,
        Queue const& present_queue,
        Surface const& surface,
        Config config
    );

    Swapchain(
        Core const& core,
        Queue const& present_queue,
        Surface const& surface,
        Config const& config,
        VkExtent2D window_extent
    );

    ~Swapchain();

    //IMPLEMENT void wrap(const Surface* surface, VkExtent2D window_extent);
    void create(VkExtent2D window_extent, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);

    /**
     * @brief destroys the swapchain
     * @note requires @ref created()
     */
    void destroy();


    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }


    void resize(VkExtent2D window_extent);



    /**
     * @return result of @ref vkAcquireNextImageKHR() [VK_SUCCESS, VK_SUBOPTIMAL_KHR]
     *
     * @note might block
     * @note the semaphore must not be signaled
     * @note the fence must be signaled
     */
    VkResult acquireNextImage(size_t in_flight_index);
    void skipAcquire(size_t in_flight_index) const;


    [[nodiscard]] VkResult present(size_t in_flight_index);
    void skipPresent(size_t in_flight_index);

    void changeSwapchainImageQueue(
        uint32_t release_queue,
        CmdBuffer const& release_cmd_buffer,
        uint32_t acquire_queue,
        CmdBuffer const& acquire_cmd_buffer,
        uint32_t image_index
    ) const;

    /**
     * @brief adds a minimum of required flags for the resolve subpass
     */
    static void addResolveSubpassDependencyFlags(VkSubpassDependency& swap_subpass_dependency);

    static void destroy(DeviceTable table, VkSwapchainKHR swapchain);

private:
    static constexpr uint32_t NO_IMAGE_INDEX = (std::numeric_limits<uint32_t>::max());

    [[nodiscard]] VkResult vkPresent(Semaphore const& semaphore, uint32_t image_index) const;

    void syncSubmit(Semaphore const* wait, Semaphore const* signal) const;

    void linkRenderFinishedSemaphores(size_t in_flight_index) const;

    void setImageFormat(VkFormat format);


    [[nodiscard]] VkSampleCountFlagBits evalMsaaSampleCount() const;


    [[nodiscard]] static VkExtent2D chooseSwapExtent(
        VkExtent2D window_extent,
        VkSurfaceCapabilitiesKHR const& capabilities
    );
    /**
     * @throws jvk::jvk_exception if no compatible image count was found
     */
    [[nodiscard]] uint32_t evalMinImageCount(uint32_t min, uint32_t max);

    void refreshSize();

    [[nodiscard]] static VkSwapchainCreateInfoKHR createInfo(
        VkSurfaceKHR surface,
        VkSurfaceFormatKHR surface_format,
        VkSurfaceCapabilitiesKHR const& capabilities,
        VkPresentModeKHR present_mode,
        VkExtent2D extent,
        uint32_t image_count,
        VkSwapchainKHR old_swapchain
    );

    /**
     * @throws jvk::vk_result_exception if vkCreateSwapchainKHR fails
     */
    void createSwapchain(VkExtent2D window_extent, VkSwapchainKHR old_swapchain);

    void createSyncObjects();


    [[nodiscard]] ImageConfig createColorImageConfig(VkSampleCountFlagBits samples) const;

    /**
     * Gets the swapchain images, must be released before destroying
     * @throws jvk::vk_result_exception if vkGetSwapchainImagesKHR fails
     */
    [[nodiscard]] std::vector<std::unique_ptr<Image>> getSwapchainImages() const;

    void createResolveAttachments();


    void createPresentInfos(std::span<Semaphore const* const> render_finished_semaphores);


    void destroyResources();

    void destroySwapchain(VkSwapchainKHR swapchain) const;

    void destroySyncObjects();

    void resizeReset();
    void reset();

    void clearImageIndices();

    not_null<Core const*> _core;
    not_null<Queue const*> _presentQueue;
    not_null<Surface const*> _surface;

    Config _config;

    move_ptr<VkSwapchainKHR_T> _handle = VK_NULL_HANDLE;



    std::unique_ptr<AttachmentCollection> _resolveAttachments;

    std::vector<Fence> _acquireFences;
    std::vector<Semaphore> _presentSemaphores;

    std::vector<PresentInfo> _presentInfos;

    VkExtent2D _extent{};
    float _aspectRatio = 0;
    VkExtent2D _windowExtent{};


    size_t _imageCount = 0;
    VkFormat _imageFormat = VK_FORMAT_UNDEFINED;


    std::vector<uint32_t> _imageIndices{};

    VkSampleCountFlagBits _msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    [[nodiscard]] Core const& core() const { return *_core; }

public:
    [[nodiscard]] auto get() const { return _handle.get(); }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] auto extentAspectRatio() const { return _aspectRatio; }

    [[nodiscard]] auto imageIndex(size_t pulse_value) const {
        CTH_CRITICAL(!cth::num::in(pulse_value, 0, _imageIndices.size()), "pulse value out of bounds") {}
        return _imageIndices[pulse_value];
    }

    [[nodiscard]] auto framesInFlight() const { return _config.framesInFlight(); }
    [[nodiscard]] auto size() const { return _imageCount; }
    [[nodiscard]] ImageConfig imageConfig() const;
    [[nodiscard]] auto imageFormat() const { return _imageFormat; }
    [[nodiscard]] auto msaaSamples() const { return _msaaSamples; }
    //TODO move this to framebuffer or render pass
    [[nodiscard]] jvk::AttachmentCollection const* resolveAttachments() const { return _resolveAttachments.get(); }
    [[nodiscard]] auto extent() const { return _extent; }

    Swapchain(Swapchain const& other) = delete;
    Swapchain& operator=(Swapchain const& other) = delete;
    Swapchain(Swapchain&& other) noexcept = default;
    Swapchain& operator=(Swapchain&& other) noexcept = default;

    static void debug_check(Swapchain const& swapchain);

    static void debug_check_window_extent(VkExtent2D window_extent);
    static void debug_check_compatibility(Swapchain const& a, Swapchain const& b);
};

}

//debug checks

namespace jvk {

inline void Swapchain::debug_check(Swapchain const& swapchain) {
    CTH_CRITICAL(swapchain._handle == VK_NULL_HANDLE, "swapchain handle invalid (VK_NULL_HANDLE)") {}
    CTH_CRITICAL(swapchain.size() == 0, "swapchain size must not be 0") {}
}

inline void Swapchain::debug_check_window_extent(VkExtent2D window_extent) {
    CTH_CRITICAL(
        window_extent.width == 0 || window_extent.height == 0,
        "window_extent width({0}) or height({0}) invalid (> 0 required)",
        window_extent.width,
        window_extent.height
    ) {}
}

inline void Swapchain::debug_check_compatibility(Swapchain const& a, Swapchain const& b) {
    CTH_CRITICAL(a._core == b._core, "swapchains not compatible (different cores)") {}
}

}
