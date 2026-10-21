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
#include <utility>
#include <vector>


namespace cvk {
class Core;
class Surface;

/**
 * @brief VkSwapchainKHR wrapper, owns its images, their views, a present semaphore and a present fence per image
 * @details the images are wrapped without memory and released before the swapchain is destroyed,
 *  @ref resize() and @ref recreate() retire the old swapchain,
 *  the swapchain destroys it once its present fences signal
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
     */
    ~Swapchain() { optDestroy(); }

    /**
     * @brief wraps the state, gets the images, creates their views, present semaphores and present fences
     * @details the format and the present mode come from the state, not from the config
     * @pre the swapchain was created for the surface with the config and the state
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::vk_result_exception result of vkGetSwapchainImagesKHR()
     */
    void wrap(State const&);

    /**
     * @brief picks the format and the present mode of the config, creates the swapchain
     * @param extent of the window, a fixed currentExtent of the surface wins
     * @details the first entries of @ref Config::formats and @ref Config::presentModes that the surface offers win
     * @pre extent width and height > 0
     * @post @ref created()
     * @note calls @ref optDestroy()
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     * @throws cvk::vk_result_exception result of vkCreateSwapchainKHR()
     */
    void create(VkExtent2D extent);

    /**
     * @brief creates a new swapchain with the old one as oldSwapchain, the old one retires
     * @param extent of the window, a fixed currentExtent of the surface wins
     * @details keeps the format and the present mode, queries the surface capabilities again,
     *  unacquires the images that are still acquired,
     *  the swapchain destroys the retired one once its present fences signal,
     *  a format or present mode change needs @ref recreate()
     * @pre @ref created()
     * @pre extent width and height > 0
     * @pre the surface still supports the format and the present mode of the swapchain
     * @pre no present semaphore is signaled without its present
     * @throws cvk::vk_result_exception result of vkCreateSwapchainKHR(), destroys the old swapchains and forgets
     */
    void resize(VkExtent2D extent);

    /**
     * @brief applies the config, e.g. another present mode, retires the old swapchain like @ref resize()
     * @param extent of the window, a fixed currentExtent of the surface wins
     * @details picks the format and the present mode of the config again, see @ref create()
     * @pre @ref created()
     * @pre extent width and height > 0
     * @pre no present semaphore is signaled without its present
     * @note a new format needs new pipelines for the swapchain images
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode, nothing changes
     * @throws cvk::vk_result_exception result of vkCreateSwapchainKHR(), destroys the old swapchains and forgets
     */
    void recreate(VkExtent2D extent, Config);

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

    [[nodiscard]] constexpr bool created() const { return !_generations.empty(); }
    [[nodiscard]] constexpr vk_handle_t get() const { return created() ? current().handle.get() : VK_NULL_HANDLE; }
    [[nodiscard]] constexpr Core const& core() const { return *_core; }
    [[nodiscard]] constexpr Config const& config() const { return _config; }

    [[nodiscard]] constexpr VkExtent2D extent() const { return _extent; }
    [[nodiscard]] constexpr VkFormat format() const { return _surfaceFormat.format; }
    [[nodiscard]] constexpr VkColorSpaceKHR colorSpace() const { return _surfaceFormat.colorSpace; }
    [[nodiscard]] constexpr VkPresentModeKHR presentMode() const { return _presentMode; }
    [[nodiscard]] constexpr uint32_t imageCount() const {
        return created() ? static_cast<uint32_t>(current().images.size()) : 0;
    }

    /**
     * @return width / height
     * @pre @ref created()
     */
    [[nodiscard]] constexpr float aspectRatio() const {
        return static_cast<float>(_extent.width) / static_cast<float>(_extent.height);
    }

    [[nodiscard]] constexpr std::span<Image const> images() const {
        return created() ? std::span<Image const>{current().images} : std::span<Image const>{};
    }
    [[nodiscard]] constexpr std::span<ImageView const> views() const {
        return created() ? std::span<ImageView const>{current().views} : std::span<ImageView const>{};
    }

    /**
     * @return the semaphore that @ref present() of the image waits for, a submit signals it
     * @pre imageIndex < @ref imageCount()
     */
    [[nodiscard]] constexpr Semaphore const& presentSemaphore(uint32_t imageIndex) const {
        return current().presentSemaphores[imageIndex];
    }

    /**
     * @pre imageIndex < @ref imageCount()
     */
    [[nodiscard]] constexpr bool acquired(uint32_t imageIndex) const { return current().acquired[imageIndex]; }

    /**
     * @return count of the retired swapchains that wait for their present fences
     */
    [[nodiscard]] constexpr size_t retired() const { return created() ? _generations.size() - 1 : 0; }

private:
    /**
     * @brief one VkSwapchainKHR with its images, views, present semaphores, present fences and acquire states
     * @details the destructor waits for the present fences and releases the images,
     *  then the members destroy in reverse order: the swapchain last,
     *  a moved from generation is empty, its destructor does nothing,
     *  no move assignment: a member wise one destroys the images without the wait and the release
     */
    struct Generation {
        explicit Generation(core_handle<Swapchain> swapchain) : handle{std::move(swapchain)} {}
        ~Generation();

        core_handle<Swapchain> handle;
        std::vector<Image> images;
        std::vector<ImageView> views;
        std::vector<Semaphore> presentSemaphores;
        std::vector<Fence> presentFences;
        std::vector<bool> acquired;

        Generation(Generation const& other) = delete;
        Generation(Generation&& other) noexcept = default;
        Generation& operator=(Generation const& other) = delete;
        Generation& operator=(Generation&& other) = delete;
    };

    /**
     * @pre @ref created()
     */
    [[nodiscard]] constexpr Generation& current() { return _generations.back(); }

    /**
     * @pre @ref created()
     */
    [[nodiscard]] constexpr Generation const& current() const { return _generations.back(); }

    /**
     * @return the first allowed format and present mode of the config that the surface offers
     * @throws cvk::cvk_exception reason: the surface offers no allowed format or present mode
     */
    [[nodiscard]] std::pair<VkSurfaceFormatKHR, VkPresentModeKHR> pick(Config const&) const;

    /**
     * @brief creates a swapchain with @ref _surfaceFormat and @ref _presentMode as the new current generation
     * @details a failure destroys every generation and forgets
     */
    void createSwapchain(VkExtent2D extent, VkSwapchainKHR oldSwapchain);

    /**
     * @brief gets the images of the current generation, creates their views, present semaphores and present fences
     */
    void createImages();

    /**
     * @brief unacquires the acquired images of the current generation
     */
    void unacquireAll();

    /**
     * @brief destroys the retired generations with signaled present fences
     */
    void collectRetired();

    void forget();

    Queue _presentQueue;
    not_null_ptr<Surface const> _surface;
    not_null_ptr<Core const> _core;
    Config _config;

    /**
     * @brief back() is the current swapchain, the others are retired, empty -> not created
     */
    std::vector<Generation> _generations;

    VkExtent2D _extent{};
    VkSurfaceFormatKHR _surfaceFormat{};
    VkPresentModeKHR _presentMode = VK_PRESENT_MODE_FIFO_KHR;

public:
    Swapchain(Swapchain const& other) = delete;
    Swapchain(Swapchain&& other) noexcept = default;
    Swapchain& operator=(Swapchain const& other) = delete;
    Swapchain& operator=(Swapchain&& other) noexcept = default;

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
