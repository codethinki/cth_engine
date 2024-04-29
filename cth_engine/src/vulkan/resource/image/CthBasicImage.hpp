#pragma once
//TEMP rename this to CthBasicImage.hpp
#include <vulkan/vulkan.h>

#include <memory>
#include <span>
#include <vector>

#include "vulkan/utility/CthConstants.hpp"

namespace cth {
class Device;
class ImageView;
class BasicBuffer;
class CmdBuffer;
class BasicMemory;
class ImageBarrier;
class DeletionQueue;


using std::vector;
using std::unique_ptr;
using std::span;


/**
 * \brief wrapper for VkImage with no ownership
 */
class BasicImage {
public:
    struct Config;
    struct State;

    BasicImage(Device* device, VkExtent2D extent, const Config& config);
    BasicImage(Device* device, VkExtent2D extent, const Config& config, VkImage vk_image, State state);
    virtual ~BasicImage() = default;

    virtual void wrap(VkImage vk_image, const State& state);

    /**
     * \brief creates the image
     * \note image must not be a valid handle
     */
    virtual void create();

    /**
     * \brief allocates image memory 
     * \param new_memory must not be allocated or nullptr
    * \note implicitly calls setMemory(new_memory)
     */
    void alloc(BasicMemory* new_memory);
    /**
     * \brief allocates image memory
     * \note memory must not be allocated
     */
    void alloc() const;

    /**
     * \brief binds buffer to new_memory
     * \param new_memory must be allocated
    * \note implicitly calls setMemory(new_memory)
     */
    void bind(BasicMemory* new_memory);
    /**
     * binds image to memory
     * \note memory must be allocated
     */
    void bind();

    /**
    * \brief destroys the image and resets the object
    * \param deletion_queue != nullptr => submit to deletion_queue
    * \note memory will not be reset
    */
    virtual void destroy(DeletionQueue* deletion_queue = nullptr);

    /**
    * \param new_memory must not be allocated, nullptr or current memory
    * \note does not free current memory
    */
    virtual void setMemory(BasicMemory* new_memory);

    /**
     * \brief copies the buffer to the image
     * \param mip_level copy dst
     * \note image must be bound & allocated
     */
    void copy(const CmdBuffer& cmd_buffer, const BasicBuffer& src_buffer, size_t src_offset = 0, uint32_t mip_level = 0) const;

    /**
     * \brief transitions the image layout via a pipeline barrier
     * \param mip_levels (Constants::ALL => all remaining)
     */
    void transitionLayout(const CmdBuffer& cmd_buffer, VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = Constants::ALL);
    /**
    * \brief adds the transition to the pipeline barrier
    * \param mip_levels (Constants::ALL => all remaining)
    */
    void transitionLayout(ImageBarrier& barrier, VkImageLayout new_layout, VkAccessFlags src_access, VkAccessFlags dst_access,
        uint32_t first_mip_level = 0, uint32_t mip_levels = Constants::ALL);


    static void destroy(const Device* device, VkImage image);

    [[nodiscard]] static uint32_t evalMipLevelCount(VkExtent2D extent);


    struct Config {
        VkImageAspectFlagBits aspectMask;
        VkFormat format;
        VkImageUsageFlags usage;
        VkImageTiling tiling;
        uint32_t mipLevels;
        VkSampleCountFlagBits samples;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        [[nodiscard]] VkImageCreateInfo createInfo() const;
    };
    struct State {
        vector<VkImageLayout> levelLayouts{}; // levelLayouts.size() < mipLevels => remaining levels are config.initialLayout
        BasicMemory* memory = nullptr; //TEMP maybe remove memory from here idk
        bool bound = memory != nullptr;
        static State Default() { return State{}; }

    private:
        void reset(const Config& config);
        void init(const Config& config);

        friend BasicImage;
    };

protected:
    virtual void reset();

    Device* device;
    VkExtent2D extent_;
    Config config_;

    State state_ = State::Default();

private:
    using transition_config = struct {
        VkAccessFlags srcAccess, dstAccess;
        VkPipelineStageFlags srcStage, vkPipelineStage;
    };
    static transition_config transitionConfig(VkImageLayout current_layout, VkImageLayout new_layout);


    void init();

    VkImage vkImage = VK_NULL_HANDLE;

    friend ImageBarrier;

public:
    [[nodiscard]] VkImage get() const { return vkImage; }
    [[nodiscard]] BasicMemory* memory() const { return state_.memory; }
    [[nodiscard]] VkFormat format() const { return config_.format; }
    [[nodiscard]] VkExtent2D extent() const { return extent_; }
    [[nodiscard]] uint32_t mipLevels() const { return config_.mipLevels; }
    [[nodiscard]] VkImageLayout layout(const uint32_t mip_level) const { return state_.levelLayouts[mip_level]; }
    [[nodiscard]] span<const VkImageLayout> layouts() const { return state_.levelLayouts; }
    [[nodiscard]] VkImageAspectFlagBits aspectMask() const { return config_.aspectMask; }
    [[nodiscard]] bool bound() const { return state_.bound; }
    [[nodiscard]] bool created() const { return vkImage != VK_NULL_HANDLE; }
    [[nodiscard]] Config config() const { return config_; }
    [[nodiscard]] State state() const { return state_; }

    BasicImage(const BasicImage& other) = delete;
    BasicImage(BasicImage&& other) = delete;
    BasicImage& operator=(const BasicImage& other) = delete;
    BasicImage& operator=(BasicImage&& other) = delete;
#ifdef _DEBUG
    static void debug_check(const BasicImage* image);
    static void debug_not_bound_check(const BasicImage* image);
#else
    static void debug_check(const BasicImage* image) {}
    static void debug_not_bound_check(const BasicImage* image) {}
#endif
};

} // namespace cth
