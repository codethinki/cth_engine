#pragma once

#include "AttachmentDescription.hpp"

#include "src/vulkan/resource/image/ImageConfig.hpp"


namespace cth::vk {

struct AttachmentCollectionConfig {
    /**
     * @brief attachments to allocate for every index
     */
    size_t attachmentsPerIndex;
    /**
     * @brief size > 0 required
     */
    std::vector<uint32_t> attachmentIndices;
    /**
     * @brief attachment image config
     */
    ImageConfig imageConfig;
    AttachmentDescription description;
};
}
