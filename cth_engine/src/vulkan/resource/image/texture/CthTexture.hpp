#pragma once
#include <span>

#include "vulkan/resource/image/CthImage.hpp"

namespace cth {

using std::span;

class Texture : public Image {
public:
    struct Config;

    /**
     * \brief automatically generates a texture with mipmaps
     * \param staging_data copied to texture via a staging buffer
     * \note the texture layout will be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     */
    Texture(Device* device, DeletionQueue* deletion_queue, VkExtent2D extent, const Config& config, const CmdBuffer& cmd_buffer,
        span<const char> staging_data);

    /**
     * \brief automatically generates a texture with mipmaps
     * \param staging_buffer staging buffer with image data
     * \note the texture layout will be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     */
    Texture(Device* device, DeletionQueue* deletion_queue, VkExtent2D extent, const Config& config, const CmdBuffer& cmd_buffer,
        const BasicBuffer& staging_buffer, size_t buffer_offset = 0);

    ~Texture() override = default;


    //void blit(const CmdBuffer* cmd_buffer);

    void init(const CmdBuffer& cmd_buffer, const BasicBuffer& buffer, size_t offset = 0);

private:
    /**
     * \brief all mip levels will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     * \param cmd_buffer
     * \param first first - 1 => src level
     * \note src must be transfer src optimal
     */
    void blitMipLevels(const CmdBuffer& cmd_buffer, int32_t first = 1, int32_t levels = 0);

    [[nodiscard]] static Image::Config imageConfig(VkExtent2D extent, const Config& config);
};
} // namespace cth


//Config

namespace cth {
struct Texture::Config {
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageAspectFlagBits aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


    [[nodiscard]] static Texture::Config Default();
};

inline cth::Texture::Config cth::Texture::Config::Default() {
    Texture::Config config;
    config.format = VK_FORMAT_R8G8B8A8_SRGB;
    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    return config;
}

}
