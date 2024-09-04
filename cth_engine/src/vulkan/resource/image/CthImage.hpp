#pragma once
#include "../memory/CthMemory.hpp"

#include "vulkan/utility/cth_constants.hpp"

#include <vulkan/vulkan.h>


namespace cth::vk {
class BasicBuffer;
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


    explicit Image(cth::not_null<BasicCore const*> core, Config const& config);

    /**
     *@brief creates the image with extent
     * @note calls @ref Image::Image()
     * @note calls @ref create() 
     */
    Image(cth::not_null<BasicCore const*> core, Config const& config, VkExtent2D extent);

    /**
     * @brief wraps an image with state
     * @note calls @ref Image::Image()
     * @note calls @ref wrap()
     */
    Image(cth::not_null<BasicCore const*> core, Config const& config, State const& state);

    virtual ~Image();

    /**
     * @brief wraps the @param state
     * @note calls @ref optDestroy()
     */
    void wrap(State const& state);

    /**
     * @brief allocates the image memory & binds it
     * @note if(@ref created()) -> calls @ref destroy()
     */
    void create(VkExtent2D extent);


    /**
    * @brief destroys and resets the object
    * @note calls @ref Memory::destroy()
    * @note pushes to destruction queue if(@ref Core::destructionQueue())
    */
    void destroy();

    /**
     * @brief calls @ref destroy() if @ref created()
     */
    void optDestroy();

    /**
     * @brief releases the ownership of image & memory
     */
    State release();

    /**
     * @brief copies the buffer to the image
     * @param mip_level copy dst
     * @note image must be bound & allocated
     */
    void copy(CmdBuffer const& cmd_buffer, BasicBuffer const& src_buffer, size_t src_offset = 0, uint32_t mip_level = 0) const;

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

    static uint32_t evalMipLevelCount(VkExtent2D  extent);


    static void destroy(VkDevice vk_device, VkImage vk_image);

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
    void createHandle();
    void alloc() const;
    void bind() const;

    void reset();

    cth::not_null<BasicCore const*> _core;
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
    [[nodiscard]] VkImageLayout layout(uint32_t  mip_level) const { return _levelLayouts[mip_level]; }
    [[nodiscard]] std::span<VkImageLayout const> layouts() const { return _levelLayouts; }
    [[nodiscard]] VkImageAspectFlagBits aspectMask() const { return _config.aspectMask; }
    [[nodiscard]] bool created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] Config config() const { return _config; }

    Image(Image const& other) = delete;
    Image(Image&& other) noexcept = default;
    Image& operator=(Image const& other) = delete;
    Image& operator=(Image&& other) noexcept = default;

    static void debug_check(cth::not_null<Image const*> image);
    static void debug_check_handle(VkImage vk_image);

#ifdef CONSTANT_DEBUG_MODE
#define DEBUG_CHECK_IMAGE(image_ptr) Image::debug_check(image_ptr)
#define DEBUG_CHECK_IMAGE_HANDLE(vk_image) Image::debug_check_handle(vk_image)
#else
#define DEBUG_CHECK_IMAGE(image_ptr) ((void)0)
#define DEBUG_CHECK_IMAGE_HANDLE(vk_image) ((void)0)
#endif
};

} // namespace cth

//State
namespace cth::vk {
struct Image::State {
    VkExtent2D extent;
    gsl::owner<VkImage> vkImage = VK_NULL_HANDLE; // NOLINT(cppcoreguidelines-owning-memory)
    std::vector<VkImageLayout> levelLayouts{}; // levelLayouts.size() < mipLevels => remaining levels are config.initialLayout
    bool bound = false;

    Memory::State memoryState{};
    static State Default() { return State{}; }
};
}

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = Constants::WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);
