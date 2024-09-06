#pragma once


#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"


#include <gsl/pointers>
#include<vector>

namespace cth::vk {
class ImageView;

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
}

namespace cth::vk {
/**
 * @brief wraps a collection of attachments of the same image for a render pass
 */
class AttachmentCollection {
public:
    struct State;

    AttachmentCollection(cth::not_null<Core const*> core, size_t size, uint32_t render_pass_index, Image::Config const& image_config,
        AttachmentDescription const& description);

    /**
     * @brief initializes the collection and creates it
     * @note calls @ref create()
     */
    AttachmentCollection(cth::not_null<Core const*> core, size_t size, uint32_t render_pass_index, Image::Config const& image_config,
        AttachmentDescription const& description, VkExtent2D extent);

    /**
     * @brief initializes the collection and wraps the state
     * @note calls @ref wrap()
     */
    AttachmentCollection(cth::not_null<Core const*> core, size_t size, uint32_t render_pass_index, Image::Config const& image_config,
        AttachmentDescription const& description, State state);


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
    void wrap(State state);

    /**
     * @brief destroys the images, memory handles and views
     * @note uses @ref Core::destructionQueue() if available
     * @note requires @ref created()
     */
    void destroy();

    /**
     * @brief releases the ownership of the images, memory handles and views
     * @return State of the Object
     * @note requires @ref created()
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

    cth::not_null<Core const*> _core;
    Image::Config _config;
    uint32_t _renderPassIndex;
    size_t _size;
    AttachmentDescription _description;

    VkExtent2D _extent{};
    std::vector<std::unique_ptr<Image>> _images;
    std::vector<std::unique_ptr<ImageView>> _views;

public:
    /**
     * @note false after moving 
     */
    [[nodiscard]] bool created() const { return !_images.empty(); }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] uint32_t index() const { return _renderPassIndex; }
    [[nodiscard]] ImageView const* view(size_t index) const;
    [[nodiscard]] Image* image(size_t index) const { return _images[index].get(); }
    [[nodiscard]] VkAttachmentDescription description() const { return _description.create(_config.format, _config.initialLayout); }
    [[nodiscard]] VkAttachmentReference reference() const {
        return VkAttachmentReference{.attachment = _renderPassIndex, .layout = _description.referenceLayout};
    }

    AttachmentCollection(AttachmentCollection const& other) = delete;
    AttachmentCollection(AttachmentCollection&& other) noexcept = default;
    AttachmentCollection& operator=(AttachmentCollection const& other) = delete;
    AttachmentCollection& operator=(AttachmentCollection&& other) noexcept = default;

#ifdef CONSTANT_DEBUG_MODE
    static void debug_check(cth::not_null<AttachmentCollection const*> collection);

#define DEBUG_CHECK_ATTACHMENT_COLLECTION(collection_ptr) AttachmentCollection::debug_check(collection_ptr)
#else
#define DEBUG_CHECK_ATTACHMENT_COLLECTION(collection_ptr) ((void)0)
#endif
};

}

//State

namespace cth::vk {
struct AttachmentCollection::State {
    // ReSharper disable once CppNonExplicitConvertingConstructor
    State(VkExtent2D extent, std::vector<unique_not_null<ImageView>> views = {}, std::vector<unique_not_null<Image>> images = {}) :
        extent{extent}, views{std::move(views)}, images{std::move(images)} {}

    /**
     * @brief attachment extent
     */
    VkExtent2D extent;

    /**
     * @brief may be empty
     * @attention requires @ref ImageView::image() at [index] == @ref images [index] get_val()
     * @attention requires @ref ImageView::created()
     */
    std::vector<unique_not_null<ImageView>> views{};

    /**
     * @attention requires @ref Image::created()
     */
    std::vector<unique_not_null<Image>> images{};

    ~State() = default;
    State(State const& other) = delete;
    State(State&& other) noexcept = default;
    State& operator=(State const& other) = delete;
    State& operator=(State&& other) noexcept = default;
};
}
