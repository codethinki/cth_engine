#pragma once
#include <vulkan/vulkan.h>

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
 * \brief image wrapper with ownership of image & memory
 */
class Image : public BasicImage {
    struct TransitionConfig;

public:
    //TEMP find a way to encapsulate the memory properties
    /**
     * \brief creates an image and allocates memory
     * \note implicitly calls create, allocate and bind
     */
    explicit Image(Device* device, DeletionQueue* deletion_queue, VkExtent2D extent, const Config& config, VkMemoryPropertyFlags memory_properties);

    ~Image() override;

    void wrap(VkImage vk_image, const State& state) override;

    /**
     * \brief creates the image
     * \note previous image will be destroyed
     */
    void create() override;


    /**
    * \brief submits image & memory to cached deletion queues and resets the object
    * \param deletion_queue != nullptr => submits to new deletion queue
    * \note new deletion queue will be cached
    * \note frees but doesn't delete memory
    */
    void destroy(DeletionQueue* deletion_queue = nullptr) override;

    void setMemory(BasicMemory* new_memory) override;

private:
    void destroyMemory(DeletionQueue* deletion_queue = nullptr);

    DeletionQueue* _deletionQueue;

public:
    Image(const Image& other) = delete;
    Image(Image&& other) = default;
    Image& operator=(const Image& other) = delete;
    Image& operator=(Image&& other) = default;
};

} // namespace cth

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = Constants::WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);
