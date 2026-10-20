#pragma once
#include "swapchain_config.hpp"

#include "cvk/base/device_table.hpp"
#include "cvk/base/queue/queue.hpp"
#include "cvk/data/img/image.hpp"
#include "cvk/data/img/image_view.hpp"
#include "cvk/render/sync/fence.hpp"
#include "cvk/render/sync/semaphore.hpp"
#include "cvk/utility/handle/core_handle.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cth/io/log.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>


namespace cvk {
class Core;
class Surface;

/**
 * @brief VkSwapchainKHR wrapper, owns its images, their views, a present semaphore and a present fence per image
 * @details the images are wrapped without memory and released before the swapchain is destroyed,
 *  @ref resize() retires the old swapchain, the swapchain destroys it once its present fences signal,
 *  custom move assignment: destroys first, a default one would destroy the swapchain images
 * @attention not thread safe
 */
class Swapchain {
public:
    using vk_handle_t = VkSwapchainKHR;
    using Config = SwapchainConfig;
    struct State;

    /**
     * @brief result of @ref acquire()
     */
    struct Acquired {
        /**
         * @brief VK_SUCCESS, VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR, VK_TIMEOUT or VK_NOT_READY
         */
        VkResult result;

        /**
         * @brief std::nullopt -> no image acquired, the semaphore stays unsignaled
         */
        std::optional<uint32_t> imageIndex;
    };

    /**
     * @brief base constructor
     * @pre @ref Core::created(), @ref Surface::created()
     * @pre the queue can present to the surface
     */
    Swapchain(Core const&, Queue const& presentQueue, Surface const&, Config);

    /**
     * @brief constructs and wraps
     * @details calls:
     * - @ref Swapchain(Core const&, Queue const&, Surface const&, Config)
     * - @ref wrap()
     */
    Swapchain(Core const&, Queue const& presentQueue, Surface const&, Config, State const&);

    /**
     * @brief constructs and creates
     * @details calls:
     * - @ref Swapchain(Core const&, Queue const&, Surface const&, Config)
     * - @ref create()
     */
    Swapchain(Core const&, Queue const& presentQueue, Surface const&, Config, VkExtent2D extent);

    /**
     * @note calls @ref optDestroy()
     * @note waits for the retired swapchains and destroys them
     */
    ~Swapchain() {
        optDestroy();
        destroyRetired();
    }

    /**
     * @brief wraps the state, gets the images, creates their views, present semaphores and present fences
     * @pre the swapchain was created for the surface with the config and the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkGetSwapchainImagesKHR()
     */
    void wrap(State const&);

    /**
     * @brief creates the swapchain with the format and present mode of the surface
     * @param extent of the window, a fixed currentExtent of the surface wins
     * @pre extent width and height > 0
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkCreateSwapchainKHR()
     */
    void create(VkExtent2D extent);

    /**
     * @brief creates a new swapchain with the old one as oldSwapchain, the old one retires
     * @param extent of the window, a fixed currentExtent of the surface wins
     * @details unacquires the images that are still acquired,
     *  the swapchain destroys the retired one once its present fences signal
     * @pre @ref created()
     * @pre extent width and height > 0
     * @pre no present semaphore is signaled without its present
     * @throws cvk::vk_result_exception result of vkCreateSwapchainKHR()
     */
    void resize(VkExtent2D extent);

    /**
     * @brief waits for the present fences, destroys the swapchain and forgets
     * @details releases the images, destroys the views, present semaphores, present fences and retired swapchains
     * @pre @ref created()
     * @pre no present semaphore is signaled without its present
     */
    void destroy();

    /**
     * @brief if @ref created() -> calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief waits for the present fences, releases ownership, returns the state and forgets
     * @details releases the images, destroys the views, present semaphores, present fences and retired swapchains
     * @pre @ref created()
     * @pre no present semaphore is signaled without its present
     */
    State release();

    /**
     * @brief acquires the next image
     * @param signal signaled when the image is ready
     * @param timeout in nanoseconds
     * @details destroys the retired swapchains with signaled present fences
     * @pre @ref created()
     * @pre signal is unsignaled and has no pending operation
     * @throws cvk::vk_result_exception other result of vkAcquireNextImageKHR()
     */
    [[nodiscard]] Acquired acquire(Semaphore const& signal, uint64_t timeout = std::numeric_limits<uint64_t>::max());

    /**
     * @brief presents the image once its present semaphore signals
     * @return VK_SUCCESS, VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR
     * @details VK_ERROR_OUT_OF_DATE_KHR -> the present still waits for the semaphore,
     *  destroys the retired swapchains with signaled present fences
     * @pre the image is acquired
     * @pre a submit signals @ref presentSemaphore() of the image
     * @throws cvk::vk_result_exception other result of vkQueuePresentKHR()
     */
    [[nodiscard]] VkResult present(uint32_t imageIndex);

    /**
     * @brief gives acquired images back without a present
     * @pre the images are acquired and all uses of them completed
     * @attention the acquire semaphores of the images still signal, wait for them before reuse
     * @throws cvk::vk_result_exception result of vkReleaseSwapchainImagesKHR()
     */
    void unacquire(std::span<uint32_t const> imageIndices);

    static void destroy(DeviceTable table, VkSwapchainKHR);

    /**
     * @return the extent clamped to the limits of the surface, a fixed currentExtent wins
     */
    [[nodiscard]] static constexpr VkExtent2D evalExtent(VkExtent2D extent, VkSurfaceCapabilitiesKHR const&);

    /**
     * @param minImageCount 0 -> minImageCount of the surface + 1
     * @return the count clamped to the limits of the surface (maxImageCount 0 => no limit)
     */
    [[nodiscard]] static constexpr uint32_t evalImageCount(uint32_t minImageCount, VkSurfaceCapabilitiesKHR const&);

    [[nodiscard]] constexpr bool created() const { return static_cast<bool>(_handle); }
    [[nodiscard]] constexpr vk_handle_t get() const { return _handle.get(); }
    [[nodiscard]] constexpr Core const& core() const { return _handle.core(); }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    [[nodiscard]] constexpr VkExtent2D extent() const { return _extent; }
    [[nodiscard]] constexpr VkFormat format() const { return _surfaceFormat.format; }
    [[nodiscard]] constexpr VkColorSpaceKHR colorSpace() const { return _surfaceFormat.colorSpace; }
    [[nodiscard]] constexpr VkPresentModeKHR presentMode() const { return _presentMode; }
    [[nodiscard]] constexpr uint32_t imageCount() const { return static_cast<uint32_t>(_images.size()); }

    /**
     * @return width / height
     * @pre @ref created()
     */
    [[nodiscard]] constexpr float aspectRatio() const {
        return static_cast<float>(_extent.width) / static_cast<float>(_extent.height);
    }

    [[nodiscard]] constexpr std::span<Image const> images() const { return _images; }
    [[nodiscard]] constexpr std::span<ImageView const> views() const { return _views; }

    /**
     * @return the semaphore that @ref present() of the image waits for, a submit signals it
     * @pre imageIndex < @ref imageCount()
     */
    [[nodiscard]] constexpr Semaphore const& presentSemaphore(uint32_t imageIndex) const {
        return _presentSemaphores[imageIndex];
    }

    /**
     * @pre imageIndex < @ref imageCount()
     */
    [[nodiscard]] constexpr bool acquired(uint32_t imageIndex) const { return _acquired[imageIndex]; }

    /**
     * @return count of the retired swapchains that wait for their present fences
     */
    [[nodiscard]] constexpr size_t retired() const { return _retired.size(); }

private:
    /**
     * @brief old swapchain after @ref resize(), the images are released
     * @details members destroy in reverse order: the swapchain last
     */
    struct Retired {
        core_handle<Swapchain> handle;
        std::vector<Image> images;
        std::vector<ImageView> views;
        std::vector<Semaphore> presentSemaphores;
        std::vector<Fence> presentFences;
    };

    void createSwapchain(VkExtent2D extent, VkSwapchainKHR oldSwapchain);
    void createImages();

    /**
     * @brief waits for the present fences, releases the images, destroys the views, semaphores and fences
     */
    void destroyImages();

    /**
     * @brief destroys the retired swapchains with signaled present fences
     */
    void collectRetired();

    /**
     * @brief waits for the present fences of the retired swapchains and destroys them
     */
    void destroyRetired();

    void forget();

    Queue _presentQueue;
    not_null_ptr<Surface const> _surface;
    Config _config;

    core_handle<Swapchain> _handle;
    std::vector<Image> _images;
    std::vector<ImageView> _views;
    std::vector<Semaphore> _presentSemaphores;
    std::vector<Fence> _presentFences;
    std::vector<bool> _acquired;
    std::vector<Retired> _retired;

    VkExtent2D _extent{};
    VkSurfaceFormatKHR _surfaceFormat{};
    VkPresentModeKHR _presentMode = VK_PRESENT_MODE_FIFO_KHR;

public:
    Swapchain(Swapchain const& other) = delete;
    Swapchain(Swapchain&& other) noexcept = default;
    Swapchain& operator=(Swapchain const& other) = delete;
    //TODO the custom move assignment is ugly, find a design where the default one works
    Swapchain& operator=(Swapchain&& other) noexcept;

    static constexpr void debug_check(Swapchain const&);
};
}

namespace cvk {
struct Swapchain::State {
    vk_not_null<VkSwapchainKHR> vkSwapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
};
}

namespace cvk {
constexpr VkExtent2D Swapchain::evalExtent(VkExtent2D extent, VkSurfaceCapabilitiesKHR const& capabilities) {
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;

    return VkExtent2D{
        std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

constexpr uint32_t Swapchain::evalImageCount(uint32_t minImageCount, VkSurfaceCapabilitiesKHR const& capabilities) {
    uint32_t const count = std::max(
        minImageCount != 0 ? minImageCount : capabilities.minImageCount + 1,
        capabilities.minImageCount
    );
    return capabilities.maxImageCount != 0 ? std::min(count, capabilities.maxImageCount) : count;
}

constexpr void Swapchain::debug_check(Swapchain const& swapchain) {
    CTH_CRITICAL(!swapchain.created(), "swapchain must be created") {}
}
}
