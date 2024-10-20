#pragma once
#include "../memory/CthMemory.hpp"

#include "vulkan/utility/cth_constants.hpp"
#include "vulkan/utility/cth_vk_types.hpp"

#include <volk.h>

namespace cth::vk {
class BaseBuffer;
class ImageBarrier;
class CmdBuffer;


/**
 * @brief image wrapper with ownership of image & memory
 */
class Image {
    struct TransitionConfig;

public:
    struct Config;
    struct State;


    /**
     * @brief base constructor
     */
    explicit Image(cth::not_null<Core const*> core, Config const& config);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref Image::Image(cth::not_null<Core const*>, Config const&)
     */
    Image(cth::not_null<Core const*> core, Config const& config, VkExtent2D extent);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref Image::Image(cth::not_null<Core const*>, Config const&)
     */
    Image(cth::not_null<Core const*> core, Config const& config, State state);

    /**
     * @brief calls @ref optDestroy()
     */
    virtual ~Image();

    /**
     * @brief wraps @ref State
     * @note calls @ref optDestroy()
     * @throws cth::vk::result_exception result of vkBindImageMemory if not bound
     * @note if not bound and not allocated calls @ref Memory::alloc()
     */
    void wrap(State state);

    /**
     * @brief allocates the image memory & binds it
     * @throws cth::vk::result_exception result of vkCreateImage
     * @throws cth::vk:.result_exception result of vkBindImageMemory
     * @note calls @ref Memory::alloc()
     * @note calls @ref optDestroy()
     */
    void create(VkExtent2D extent);


    /**
    * @brief destroys and resets the object
    * @attention requires @ref created()
    * @note calls @ref Memory::destroy()
    * @note pushes to @ref Core::destructionQueue() if available
    */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases the ownership and resets
     * @attention requires @ref created()
     */
    State release();

    /**
     * @brief copies the buffer to the image
     * @param mip_level copy dst
     * @note image must be bound & allocated
     */
    void copy(CmdBuffer const& cmd_buffer, BaseBuffer const& src_buffer, size_t src_offset = 0, uint32_t mip_level = 0) const;

    /**
     * @brief transitions the image layout via a pipeline barrier
     * @param mip_levels (Constants::ALL => all remaining)
     */
    void transitionLayout(CmdBuffer const& cmd_buffer, VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = constants::ALL);
    /**
    * @brief adds the transition to the pipeline barrier
    * @param mip_levels (Constants::ALL => all remaining)
    */
    void transitionLayout(ImageBarrier& barrier, VkImageLayout new_layout, VkAccessFlags src_access, VkAccessFlags dst_access,
        uint32_t first_mip_level = 0, uint32_t mip_levels = constants::ALL);

    static uint32_t evalMipLevelCount(VkExtent2D extent);


    static void destroy(vk::not_null<VkDevice> vk_device, VkImage vk_image);

    struct Config {
        VkImageAspectFlagBits aspectMask;
        VkFormat format;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags memoryProperties;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        uint32_t mipLevels = 1;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        [[nodiscard]] VkImageCreateInfo createInfo() const;
    };

protected:
    std::vector<VkImageLayout> _levelLayouts;

private:
    /**
     * @throws cth::vk::result_exception result of vkCreateImage
     */
    void createHandle();

    /**
     * @note calls @ref Memory::alloc()
     */
    void alloc() const;

    /**
     * @throws cth::vk::result_exception result of vkBindImageMemory
     */
    void bind() const;

    void reset();

    cth::not_null<Core const*> _core;
    VkExtent2D _extent;
    Config _config;


    struct TransitionConfig {
        VkAccessFlags srcAccess, dstAccess;
        VkPipelineStageFlags srcStage, vkPipelineStage;

        static TransitionConfig Create(VkImageLayout current_layout, VkImageLayout new_layout);
    };



    move_ptr<VkImage_T> _handle = VK_NULL_HANDLE;
    std::unique_ptr<Memory> _memory = nullptr;

    friend ImageBarrier;

public:
    [[nodiscard]] VkImage get() const { return _handle.get(); }
    [[nodiscard]] Memory* memory() const { return _memory.get(); }
    [[nodiscard]] VkFormat format() const { return _config.format; }
    [[nodiscard]] VkExtent2D extent() const { return _extent; }
    [[nodiscard]] uint32_t mipLevels() const { return _config.mipLevels; }
    [[nodiscard]] VkImageLayout layout(uint32_t mip_level) const { return _levelLayouts[mip_level]; }
    [[nodiscard]] std::span<VkImageLayout const> layouts() const { return _levelLayouts; }
    [[nodiscard]] VkImageAspectFlagBits aspectMask() const { return _config.aspectMask; }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] Config config() const { return _config; }

    Image(Image const& other) = delete;
    Image& operator=(Image const& other) = delete;
    Image(Image&& other) noexcept = default;
    Image& operator=(Image&& other) noexcept = default;

    static void debug_check(cth::not_null<Image const*> image);
    static void debug_check_handle(vk::not_null<VkImage> vk_image);
};

} // namespace cth

//State
namespace cth::vk {
struct Image::State {
    State(VkExtent2D extent, vk::not_null<VkImage> vk_image, bool const bound, std::unique_ptr<Memory> memory,
        std::vector<VkImageLayout> level_layouts = {}) : extent{extent}, vkImage{vk_image},
        bound{bound}, memory{std::move(memory)}, levelLayouts{std::move(level_layouts)} {}

    VkExtent2D extent;
    vk::not_null<VkImage> vkImage;
    /**
   * @brief if true and @ref memory == nullptr -> memory unknown
   */
    bool bound;

    /**
     * @brief may be nullptr
     */
    std::unique_ptr<Memory> memory;
    std::vector<VkImageLayout> levelLayouts{}; // levelLayouts.size() < mipLevels => remaining levels are config.initialLayout

    ~State() = default;
    State(State const& other) = delete;
    State& operator=(State const& other) = delete;
    State(State&& other) noexcept = default;
    State& operator=(State&& other) noexcept = default;
};
}

//debug checks

namespace cth::vk {
inline void Image::debug_check(cth::not_null<Image const*> image) {
    CTH_CRITICAL(!image->created(), "image must be created") {}
    debug_check_handle(image->get());
}
inline void Image::debug_check_handle([[maybe_unused]] vk::not_null<VkImage> vk_image) {}
}

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = Constants::WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);

