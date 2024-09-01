#pragma once
#include "../CthDescriptor.hpp"

#include "vulkan/resource/image/CthImage.hpp"
#include "vulkan/resource/image/CthImageView.hpp"
#include "vulkan/resource/image/texture/CthSampler.hpp"

namespace cth::vk {
class ImageDescriptor : public Descriptor {
public:
    explicit ImageDescriptor(VkDescriptorType type, VkDescriptorImageInfo const& info) : Descriptor(type), _vkDescriptorInfo(info) {}
    ~ImageDescriptor() override = 0;

private:
    VkDescriptorImageInfo _vkDescriptorInfo{};

public:
    [[nodiscard]] VkDescriptorImageInfo imageInfo() const override { return _vkDescriptorInfo; }

    ImageDescriptor(ImageDescriptor const& other) = default;
    ImageDescriptor(ImageDescriptor&& other) = delete;
    ImageDescriptor& operator=(ImageDescriptor const& other) = default;
    ImageDescriptor& operator=(ImageDescriptor&& other) = delete;
};
inline ImageDescriptor::~ImageDescriptor() = default;



class StorageImageDescriptor : public ImageDescriptor {
public:
    static constexpr VkDescriptorType TYPE = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    explicit StorageImageDescriptor(ImageView const* image_view) : ImageDescriptor(TYPE,
        VkDescriptorImageInfo{VK_NULL_HANDLE, image_view->get(), image_view->image()->layout(0)}) {}
};

class TextureDescriptor : public ImageDescriptor {
public:
    static constexpr VkDescriptorType TYPE = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    explicit TextureDescriptor(ImageView const* image_view, Sampler const* sampler) : ImageDescriptor(TYPE,
        VkDescriptorImageInfo{sampler->get(), image_view->get(), image_view->image()->layout(0)}) {}
};



} // namespace cth
