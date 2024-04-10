#pragma once
#include <vulkan/vulkan.h>

#include <vector>

#include "CthBasicImage.hpp"

namespace cth {
using namespace std;

class Device;
class DefaultBuffer;
class Sampler;
class ImageView;
class Image;


class Image : public BasicImage {
    struct TransitionConfig;

public:
    explicit Image(Device* device, VkExtent2D extent, const Config& config);
    ~Image() override;


    void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0) const;

    /**
     * \brief transitions the image layout via a pipeline barrier
     * \param mip_levels (0 => all remaining)
     */
    void transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = 0);

    [[nodiscard]] static uint32_t evalMipLevelCount(VkExtent2D extent);

private:
    void create();
    void allocate();
    void bind() const;

    using transition_config = struct {
        VkAccessFlags srcAccess, dstAccess;
        VkPipelineStageFlags srcStage, vkPipelineStage;
    };
    static transition_config transitionConfig(VkImageLayout current_layout, VkImageLayout new_layout);



    VkDeviceMemory vkImageMemory = VK_NULL_HANDLE;
protected:
    Device* device;
};

} // namespace cth

//TODO implement multidimensional image support
//void write(const DefaultBuffer* buffer, size_t offset = 0, uint32_t mip_level = 0, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE) const;
//transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level = 0, uint32_t mip_levels = VK_WHOLE_SIZE, VkImageAspectFlagBits aspect_mask = VK_IMAGE_ASPECT_NONE);
