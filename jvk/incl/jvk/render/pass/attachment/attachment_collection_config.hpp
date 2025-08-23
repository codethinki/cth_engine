#pragma once

#include "attachment_description.hpp"

#include "jvk/res/img/image_config.hpp"


namespace jvk {

struct AttachmentCollectionConfig {


    AttachmentCollectionConfig(size_t attachments_per_index, std::span<uint32_t const> attachment_indices,
        ImageConfig const& image_config,
        AttachmentDescription const& description) : attachmentsPerIndex{attachments_per_index},
        attachmentIndices{std::from_range, attachment_indices},
        imageConfig{image_config}, description{description} {}


    AttachmentCollectionConfig(size_t attachments, uint32_t index, ImageConfig const& image_config,
        AttachmentDescription const& description) : AttachmentCollectionConfig{attachments,
        std::vector{index}, image_config, description} {}

    /**
     * @brief attachments to allocate for every index
     */
    size_t attachmentsPerIndex;
    /**
     * @brief render pass attachment indices to allocate for
     * @details size > 0 required
     */
    std::vector<uint32_t> attachmentIndices;
    /**
     * @brief attachment image config
     */
    ImageConfig imageConfig;
    AttachmentDescription description;


    static void debug_check(AttachmentCollectionConfig& config) {
        CTH_WARN(!std::ranges::is_sorted(config.attachmentIndices),
            "attachment indices are not sorted but will be in the collection") {}

        CTH_CRITICAL(config.attachmentIndices.empty(),
            "there must at least be one subpass attachment index") {}
    }

};
}
