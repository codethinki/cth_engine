#pragma once


#include "attachment_collection_config.hpp"

#include "jvk/res/img/image.hpp"
#include "jvk/res/img/image_view.hpp"


#include <vector>



namespace jvk {
struct AttachmentDescription;

/**
 * @brief wraps a collection of attachments of the same image for a render pass
 */
class AttachmentCollection {
public:
    using Config = AttachmentCollectionConfig;

    struct State;

    /**
     * @brief base constructor
     */
    AttachmentCollection(Core const& core, Config config);

    /**
     * @brief initializes the collection and creates it
     * @note calls @ref create()
     */
    AttachmentCollection(Core const& core, Config const& config, VkExtent2D extent);

    /**
     * @brief initializes the collection and wraps the state
     * @note calls @ref wrap()
     */
    AttachmentCollection(Core const& core, Config const& config, State state);


    ~AttachmentCollection();


    /**
     * @brief creates the attachment images and views
     * @note calls @ref Image::create() and @ref ImageView::create()
     * @note calls optDestroy()
     */
    void create(VkExtent2D extent);

    /**
     * @brief wraps an existing state
     * @note @ref State::views can be empty -> views will be created
     * @note calls @ref optDestroy()
     */
    void wrap(State state);

    /**
     * @brief destroys the images, memory handles and views
     * @attention requires @ref created()
     * @note uses @ref Core::destructionQueue() if available
     */
    void destroy();

    /**
     * @brief if @ref created() calls @ref destroy()
     */
    void optDestroy() { if(created()) destroy(); }

    /**
     * @brief releases the ownership of the images, memory handles and views
     * @return State of the Object
     * @note requires @ref created()
     */
    [[nodiscard]] State release();

private:
    void init();

    void reset();
    void createImages();
    void createImageViews();

    [[nodiscard]] size_t index_of(size_t sub_index, size_t index_nr) const;
    [[nodiscard]] size_t total_size() const { return _config.attachmentsPerIndex * attachments(); }

    not_null<Core const*> _core;
    Config _config;

    VkExtent2D _extent{};
    std::vector<std::unique_ptr<Image>> _images;
    std::vector<ImageView> _views;

public:
    /**
     * @note false after moving
     */
    [[nodiscard]] bool created() const;

    [[nodiscard]] size_t per_attachment_size() const { return _config.attachmentsPerIndex; }
    [[nodiscard]] size_t attachments() const { return _config.attachmentIndices.size(); }
    [[nodiscard]] std::span<uint32_t const> indices() const { return _config.attachmentIndices; }

    /**
     * @brief gets the image view at @ref index for the @ref attachment_nr
     * @param sub_index of a render pass attachment index in collection
     * @param att_index render pass attachment index collection index (2 in [0, 2, 3] => 1)
     */
    [[nodiscard]] ImageView const& view(size_t sub_index, size_t att_index = 0) const;
    /**
     * @brief gets the image at @ref index for the @ref attachment_nr
     * @param sub_index of a render pass attachment index in collection
     * @param att_index render pass attachment index collection index (2 in [0, 2, 3] => 1)
     */
    [[nodiscard]] Image* image(size_t sub_index, size_t att_index = 0) const;


    [[nodiscard]] VkAttachmentDescription description() const {
        return _config.description.create(_config.imageConfig.format, _config.imageConfig.initialLayout);
    }

    /**
     * @return attachment reference for every render pass attachment index
     */
    [[nodiscard]] std::vector<VkAttachmentReference> references(uint32_t subpass_index) const;

    AttachmentCollection(AttachmentCollection const& other) = delete;
    AttachmentCollection(AttachmentCollection&& other) noexcept = default;
    AttachmentCollection& operator=(AttachmentCollection const& other) = delete;
    AttachmentCollection& operator=(AttachmentCollection&& other) noexcept = default;

    static void debug_check(AttachmentCollection const& collection);
};

}


//State

namespace jvk {
// TODO see if this can be refactored to just vectors of Image/ImageView
struct AttachmentCollection::State {
    explicit State(
        VkExtent2D extent,
        std::vector<unique_not_null<ImageView>> views = {},
        std::vector<unique_not_null<Image>> images = {}
    ) : extent{extent},
        views{std::move(views)},
        images{std::move(images)} {}

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


//debug checks

namespace jvk {
inline void AttachmentCollection::debug_check(AttachmentCollection const& collection) {
    CTH_CRITICAL(!collection.created(), "collection must have been created") throw details->exception();
}
}
