#pragma once
#include <vulkan/vulkan.h>

#include <vector>

#include "CthBasicImage.hpp"

namespace cth {
class Memory;
class Device;
class BasicBuffer;
class Sampler;
class ImageView;
class Image;
class DeletionQueue;
class CmdBuffer;


/**
 * \brief image wrapper with implicit ownership
 */
class Image : public BasicImage {
    struct TransitionConfig;

public:
    //TEMP find a way to encapsulate the memory properties
    /**
     * \brief creates an image with memory
     */
    explicit Image(Device* device, DeletionQueue* deletion_queue, VkExtent2D extent, const Config& config, VkMemoryPropertyFlags memory_properties);

    /**
     * \brief creates an image
     * \param memory can be allocated, takes ownership
     */
    explicit Image(Device* device, DeletionQueue* deletion_queue, VkExtent2D extent, const Config& config, unique_ptr<BasicMemory> memory);
    ~Image() override;

    [[nodiscard]] static uint32_t evalMipLevelCount(VkExtent2D extent);

    /**
     * \brief adds image to the deletion queue
     * \brief frees the memory
     */
    void destroy() override;

private:
    DeletionQueue* deletionQueue;
};

} // namespace cth

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = VK_WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);
