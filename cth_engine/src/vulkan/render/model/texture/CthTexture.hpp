#pragma once
#include "vulkan/memory/image/CthImage.hpp"

namespace cth {

class Texture : public Image {
public:
    struct Config;

    /**
     * \brief automatically generates a texture with mipmaps
     * \note the texture will be created with VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     * \param staging_data copied to texture via a staging buffer
     */
    Texture(Device* device, VkExtent2D extent, const Config& config, span<const char> staging_data);

    /**
     * \brief automatically generates a texture with mipmaps
     * \note the texture will be created with VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     * \param staging_buffer staging buffer with image data
     */
    Texture(Device* device, VkExtent2D extent, const Config& config, const DefaultBuffer* staging_buffer, size_t buffer_offset = 0);
    ~Texture() override = default;

private:
    void init(const DefaultBuffer* buffer, size_t offset = 0);

    /**
    * \brief blits the image onto to the mipmaps
    * \note first - 1 to first + levels - 1 will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    */
    void genMipmaps(int32_t first = 1, int32_t levels = 0);

    [[nodiscard]] static Image::Config imageConfig(VkExtent2D extent, const Config& config);

    //hidden methods
    void write(const DefaultBuffer* buffer, size_t offset, uint32_t mip_level) const;
    void transitionLayout(VkImageLayout new_layout, uint32_t first_mip_level, uint32_t mip_levels);
};
} // namespace cth


//Config

namespace cth {
struct Texture::Config {
    VkImageAspectFlagBits aspectMask;
    VkFormat format;
};

}
