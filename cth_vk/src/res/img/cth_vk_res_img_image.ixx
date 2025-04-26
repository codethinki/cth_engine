module;
#include "lib/volk.hpp"
#include <cth/io/io_log.hpp>
export module cth.vk.res.img.image;

import cth.vk.base.device_table;
import cth.vk.constants;
import cth.vk.res.buffer.base;
import cth.vk.render.rec.cmd.buffer.base;
import cth.vk.util.types;
import cth.vk.render.sync.barrier.base;
import cth.vk.res.memory;
import cth.vk.base.core;

import cth.io.log;

export namespace cth::vk {
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
    explicit Image(Core const& core, Config const& config);

    /**
     * @brief constructs and calls @ref create()
     * @note calls @ref Image::Image(Core const&, Config const&)
     */
    Image(Core const& core, Config const& config, VkExtent2D extent);

    /**
     * @brief constructs and calls @ref wrap(State const&)
     * @note calls @ref Image::Image(Core const&, Config const&)
     */
    Image(Core const& core, Config const& config, State state);

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

    static uint32_t evalMipLevelCount(VkExtent2D extent);


    static void destroy(DeviceTable table, VkImage vk_image);

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
    [[nodiscard]] Core const& core() const { return *_core; }
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

    VkExtent2D _extent{};

    cth::not_null<Core const*> _core;
    Config _config;


    struct TransitionConfig {
        VkAccessFlags srcAccess, dstAccess;
        VkPipelineStageFlags srcStage, vkPipelineStage;

        static TransitionConfig Create(VkImageLayout current_layout, VkImageLayout new_layout);
    };



    move_ptr<VkImage_T> _handle = VK_NULL_HANDLE;
    std::unique_ptr<Memory> _memory = nullptr;

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

    static void debug_check(Image const& image);
    static void debug_check_handle(vk::not_null<VkImage> vk_image);
    static void debug_check_layout_consistency(Image const& image, ptrdiff_t first_mip_level, ptrdiff_t mip_levels);
};

} // namespace cth

//State
export namespace cth::vk {
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

export namespace cth::vk {
inline void Image::debug_check(Image const& image) {
    CTH_CRITICAL(!image.created(), "image must be created") {}
    debug_check_handle(image.get());
}
inline void Image::debug_check_handle([[maybe_unused]] vk::not_null<VkImage> vk_image) {}

inline void Image::debug_check_layout_consistency(Image const& image, ptrdiff_t first_mip_level, ptrdiff_t mip_levels) {
    auto& levelLayouts = image._levelLayouts;

    auto const oldLayout = levelLayouts[first_mip_level];
    CTH_CRITICAL(
        std::any_of(levelLayouts.begin() + first_mip_level,
            mip_levels == constants::ALL ? levelLayouts.end() : levelLayouts.begin() + first_mip_level + mip_levels,
            [oldLayout](VkImageLayout layout) { return oldLayout != layout; }), "all transitioned layouts must be the same"
    ) {}
}
}

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = Constants::WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);
