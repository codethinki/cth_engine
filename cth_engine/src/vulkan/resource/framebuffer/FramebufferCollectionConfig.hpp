#pragma once
#include "src/vulkan/utility/cth_constants.hpp"

namespace cth::vk {
class BasicSwapchain;
}

namespace cth::vk {
class ImageView;
class AttachmentCollection;

struct FramebufferCollectionConfig {
private:
    using attachments_span_t = std::span<AttachmentCollection const* const>;

public:
    using attachment_map_t = std::unordered_map<AttachmentCollection const*, std::vector<size_t>>;
    using views_t = std::vector<ImageView const*>;

    /**
     * @brief contains [attachments_for_framebuffer1, ..., attachments_for_framebuffer_n]
     * @attention must be divisible by @ref framebuffers
     */
    views_t imageViews;


    /**
     * @brief framebuffer size for each framebuffer
     * @attention must be > 0
     */
    size_t framebufferSize = constants::FRAMES_IN_FLIGHT;

    /**
     * @param framebuffer_attachments using the same collection n times will result in an offset of usages_so_far * [@ref size()] in the collection for each usage
     * @param max_collection_size actual size will be min{max_size, attachment[n].size() / attachment_usages | 0 <= n < [attachments.size()] }
     */
    static FramebufferCollectionConfig Attachments(attachments_span_t framebuffer_attachments, size_t max_collection_size = constants::FRAMES_IN_FLIGHT);

    //TEMP left off here, complete the collection config with a Swapchain constructor that takes the swapchain and the attachment collections and
    //matches them so every swapchain image index has one complete set of framebuffers
    // also rewirte the framebuffer collection to accept this new config

private:
    static [[nodiscard]] attachment_map_t attachmentMap(attachments_span_t attachments);
  
    static [[nodiscard]] size_t minSize(attachments_span_t attachments, size_t max_size);

    static [[nodiscard]] views_t readViews(attachment_map_t const& map, size_t size, size_t attachments);

public:
    static void debug_check(FramebufferCollectionConfig const& config) {
        CTH_CRITICAL(config.framebufferSize == 0, "config framebuffers must be > 0") {}
        CTH_CRITICAL(config.imageViews.size() % config.framebufferSize != 0,
            "imageViews [{}] % framebuffers [{}] == 0 required", config.imageViews.size(), config.framebufferSize) {}
    }
};
}
