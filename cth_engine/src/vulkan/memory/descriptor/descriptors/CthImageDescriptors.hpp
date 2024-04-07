#pragma once
#include "../CthDescriptor.hpp"

#include "vulkan/memory/image/CthImage.hpp"
#include "vulkan/memory/image/CthImageView.hpp"
#include "vulkan/render/model/texture/CthSampler.hpp"

namespace cth {
class ImageDescriptor : public Descriptor {
public:
    explicit ImageDescriptor(const VkDescriptorType type, const VkDescriptorImageInfo& info, const size_t descriptor_size = VK_WHOLE_SIZE,
        const size_t image_offset = 0) : Descriptor(type, descriptor_size, image_offset), vkDescriptorInfo(info) {}
    ~ImageDescriptor() override = 0;

private:
    VkDescriptorImageInfo vkDescriptorInfo{};

public:
    [[nodiscard]] VkDescriptorImageInfo imageInfo() const override { return vkDescriptorInfo; }

    ImageDescriptor(const ImageDescriptor& other) = default;
    ImageDescriptor(ImageDescriptor&& other) = delete;
    ImageDescriptor& operator=(const ImageDescriptor& other) = default;
    ImageDescriptor& operator=(ImageDescriptor&& other) = delete;
};
inline ImageDescriptor::~ImageDescriptor() = default;



class StorageImageDescriptor : public ImageDescriptor {
public:
    inline static constexpr VkDescriptorType TYPE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    explicit StorageImageDescriptor(const ImageView* image_view, const size_t descriptor_size,
        const size_t descriptor_offset) : ImageDescriptor(TYPE,
        VkDescriptorImageInfo{VK_NULL_HANDLE, image_view->get(), image_view->image()->layout()},
        descriptor_size, descriptor_offset) {}
};

class TextureDescriptor : public ImageDescriptor {
public:
    inline static constexpr VkDescriptorType TYPE = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    explicit TextureDescriptor(const ImageView* image_view, const Sampler* sampler, const size_t descriptor_size,
        const size_t descriptor_offset) : ImageDescriptor(TYPE,
        VkDescriptorImageInfo{sampler->get(), image_view->get(), image_view->image()->layout()},
        descriptor_size, descriptor_offset) {}
};



} // namespace cth
