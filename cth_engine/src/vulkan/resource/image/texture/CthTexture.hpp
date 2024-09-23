#pragma once
#include <span>

#include "vulkan/resource/image/CthImage.hpp"

//TEMP modernize

namespace cth::vk {

class Texture : public Image {
public:
    struct Config;

    /**
     * @brief automatically generates a texture with mipmaps
     * @param staging_data copied to texture via a staging buffer
     * @note the texture layout will be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     */
    Texture(cth::not_null<Core const*> core, VkExtent2D extent, Config const& config, CmdBuffer const& cmd_buffer,
        std::span<char const> staging_data);

    /**
     * @brief automatically generates a texture with mipmaps
     * @param staging_buffer staging buffer with image data
     * @note the texture layout will be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     */
    Texture(cth::not_null<Core const*> core, VkExtent2D extent, Config const& config, CmdBuffer const& cmd_buffer,
        BaseBuffer const& staging_buffer, size_t buffer_offset = 0);

    ~Texture() override = default;


    //void blit(const CmdBuffer* cmd_buffer);

    void init(CmdBuffer const& cmd_buffer, BaseBuffer const& buffer, size_t offset = 0);

private:
    /**
     * @brief all mip levels will be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     * @param cmd_buffer
     * @param first first - 1 => src level
     * @note src must be transfer src optimal
     */
    void blitMipLevels(CmdBuffer const& cmd_buffer, uint32_t first = 1, uint32_t levels = 0);

    [[nodiscard]] static Image::Config imageConfig(VkExtent2D extent, Config const& config);
};
} // namespace cth


//Config

namespace cth::vk {
struct Texture::Config {
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    VkImageAspectFlagBits aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;


    [[nodiscard]] static Texture::Config Default();
};

inline Texture::Config Texture::Config::Default() {
    Texture::Config config;
    config.format = VK_FORMAT_R8G8B8A8_SRGB;
    config.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    return config;
}

}
