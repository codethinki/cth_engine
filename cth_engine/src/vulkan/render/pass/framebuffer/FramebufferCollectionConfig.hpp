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
     * @brief collection framebuffers
     * @attention must be > 0
     */
    size_t framebuffers = constants::FRAMES_IN_FLIGHT;

    /**
     * @brief creates with attachments at in collection specified 
     */
    static FramebufferCollectionConfig Attachments(attachments_span_t collections, size_t max_framebuffers = constants::ALL);

    //TEMP left off here, complete the collection config with a Swapchain constructor that takes the swapchain and the attachment collections and
    //matches them so every swapchain image index has one complete set of framebuffers
    // also rewirte the framebuffer collection to accept this new config

private:
    static [[nodiscard]] size_t evalFramebufferCount(attachments_span_t collections, size_t max);

    static [[nodiscard]] views_t readViews(attachments_span_t const& collections, size_t framebuffers);

public:
    static void debug_check(FramebufferCollectionConfig const& config) {
        CTH_CRITICAL(config.framebuffers == 0, "config framebuffers must be > 0") {}
        CTH_CRITICAL(config.imageViews.size() % config.framebuffers != 0,
            "imageViews [{}] % framebuffers [{}] == 0 required", config.imageViews.size(), config.framebuffers) {}
    }
};
}
