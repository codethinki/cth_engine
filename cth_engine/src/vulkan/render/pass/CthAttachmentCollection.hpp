#pragma once
#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"

#include <gsl/pointers>

namespace cth::vk {

struct AttachmentDescription {
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout referenceLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAttachmentDescriptionFlags flags = 0;

    [[nodiscard]] VkAttachmentDescription create(VkFormat format, VkImageLayout initial_layout) const;
};
/**
 * @brief wraps a collection of attachments of the same image for a render pass
 */
class AttachmentCollection {
public:
    struct State;

    AttachmentCollection(not_null<BasicCore const*> core, size_t size, uint32_t render_pass_index, Image::Config const& image_config,
        AttachmentDescription const& description);

    /**
     * @brief initializes the collection and creates it
     * @note calls @ref create()
     */
    AttachmentCollection(not_null<BasicCore const*> core, size_t size, uint32_t render_pass_index, Image::Config const& image_config,
        AttachmentDescription const& description, VkExtent2D extent);

    /**
     * @brief initializes the collection and wraps the state
     * @note calls @ref wrap()
     */
    AttachmentCollection(not_null<BasicCore const*> core, size_t size, uint32_t render_pass_index, Image::Config const& image_config,
        AttachmentDescription const& description, State const& state);


    ~AttachmentCollection();


    /**
     * @brief creates the attachment images and views
     *
     */
    void create(VkExtent2D extent);

    /**
     * @brief wraps an existing state
     * @note @ref State::views can be empty -> views will be created
     * @note calls @ref destroy() if @ref created()
     */
    void wrap(State const& state);

    /**
     * @brief destroys the images, memory handles and views
     * @note uses @ref BasicCore::destructionQueue() if available
     * @note @ref created() required
     */
    void destroy();

    /**
     * @brief releases the ownership of the images, memory handles and views
     * @return State of the Object
     * @note @ref created() required
     */
    [[nodiscard]] State release();

    /**
     * @brief destroys if @ref created()
     * @note may call @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

private:
    void reset();
    void createImages();
    void createImageViews();

    not_null<BasicCore const*> _core;
    Image::Config _config;
    uint32_t _renderPassIndex;
    size_t _size;
    AttachmentDescription _description;

    VkExtent2D _extent{};
    std::vector<Image> _images;
    std::vector<ImageView> _views;

public:
    /**
     * @note false after moving 
     */
    [[nodiscard]] bool created() const { return !_images.empty(); }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] uint32_t index() const { return _renderPassIndex; }
    [[nodiscard]] ImageView const* view(size_t index) const { return &_views[index]; }
    [[nodiscard]] Image* image(size_t index) { return &_images[index]; }
    [[nodiscard]] VkAttachmentDescription description() const { return _description.create(_config.format, _config.initialLayout); }
    [[nodiscard]] VkAttachmentReference reference() const {
        return VkAttachmentReference{.attachment = _renderPassIndex, .layout = _description.referenceLayout};
    }

    AttachmentCollection(AttachmentCollection const& other) = delete;
    AttachmentCollection(AttachmentCollection&& other) noexcept = default;
    AttachmentCollection& operator=(AttachmentCollection const& other) = delete;
    AttachmentCollection& operator=(AttachmentCollection&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(AttachmentCollection const* collection);

#define DEBUG_CHECK_ATTACHMENT_COLLECTION(collection_ptr) AttachmentCollection::debug_check(collection_ptr)
#else
#define DEBUG_CHECK_ATTACHMENT_COLLECTION(collection_ptr) ((void)0)
#endif
};

}

//State

namespace cth::vk {
struct AttachmentCollection::State {
    VkExtent2D extent; //extent of @ref vkImages
    std::vector<gsl::owner<VkImage>> vkImages; ///VkImage handles bound to @ref vkMemoryHandles

    size_t byteSize; //size of @ref vkMemoryHandles
    /**
     * @brief VkDeviceMemory handles bound to @ref vkImages
     * @note empty -> bound but unknown memory
     */
    std::vector<gsl::owner<VkDeviceMemory>> vkMemoryHandles;
    /**
     * @brief VkImageView handles bound to @ref vkImages
     * @note empty -> views will be created
     */
    std::vector<gsl::owner<VkImageView>> vkImageViews;
};
}
