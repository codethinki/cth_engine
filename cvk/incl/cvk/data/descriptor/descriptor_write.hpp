#pragma once
#include "cvk/utility/range.hpp"
#include "cvk/utility/types.hpp"

#include <volk.h>

#include <cstdint>
#include <variant>


namespace cvk {
class BaseBuffer;
class DescriptorSet;
class ImageView;
class Sampler;

/**
 * @brief one descriptor of one binding, the constructor decides the kind
 * @details the binding type in the layout of the set decides the descriptor type and the default image layout:
 *  storage image -> VK_IMAGE_LAYOUT_GENERAL, input attachment -> VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,
 *  else VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 */
class DescriptorWrite {
public:
    /**
     * @brief buffer descriptor: uniform or storage buffer, dynamic or not
     * @param range in bytes
     * @param element array element of the binding
     */
    constexpr DescriptorWrite(uint32_t binding, BaseBuffer const& buffer, Range range = {}, uint32_t element = 0) :
        _binding{binding}, _element{element}, _resource{BufferResource{&buffer, range}} {}

    /**
     * @brief image descriptor without sampler: sampled image, storage image, input attachment
     * @param element array element of the binding
     * @param layout of the view at the gpu access (VK_IMAGE_LAYOUT_MAX_ENUM => the default of the binding type)
     */
    constexpr DescriptorWrite(
        uint32_t binding,
        ImageView const& view,
        uint32_t element = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_MAX_ENUM
    ) : _binding{binding}, _element{element}, _resource{ImageResource{&view, nullptr, layout}} {}

    /**
     * @brief combined image sampler descriptor
     * @param element array element of the binding
     * @param layout of the view at the gpu access (VK_IMAGE_LAYOUT_MAX_ENUM => the default of the binding type)
     */
    constexpr DescriptorWrite(
        uint32_t binding,
        ImageView const& view,
        Sampler const& sampler,
        uint32_t element = 0,
        VkImageLayout layout = VK_IMAGE_LAYOUT_MAX_ENUM
    ) : _binding{binding}, _element{element}, _resource{ImageResource{&view, &sampler, layout}} {}

    /**
     * @brief sampler descriptor
     * @param element array element of the binding
     */
    constexpr DescriptorWrite(uint32_t binding, Sampler const& sampler, uint32_t element = 0) :
        _binding{binding}, _element{element}, _resource{ImageResource{nullptr, &sampler, VK_IMAGE_LAYOUT_UNDEFINED}} {}

private:
    struct BufferResource {
        not_null_ptr<BaseBuffer const> buffer;
        Range range;
    };

    /**
     * @brief view nullptr -> sampler descriptor, sampler nullptr -> no sampler
     */
    struct ImageResource {
        ImageView const* view;
        Sampler const* sampler;
        VkImageLayout layout;
    };

    uint32_t _binding;
    uint32_t _element;
    std::variant<BufferResource, ImageResource> _resource;

    friend DescriptorSet;
};
}
