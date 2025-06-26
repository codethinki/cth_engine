#include "FramebufferCollectionConfig.hpp"

#include "src/vulkan/render/pass/AttachmentCollection.hpp"

namespace cth::vk {

FramebufferCollectionConfig FramebufferCollectionConfig::Attachments(attachments_span_t framebuffer_attachments, size_t max_collection_size) {
    auto const map = attachmentMap(framebuffer_attachments);
    auto const framebuffers = minSize(framebuffer_attachments, max_collection_size);

    return {
        .imageViews = readViews(map, framebuffers, framebuffer_attachments.size()),
        .framebufferSize = framebuffer_attachments.size(),
    };
}
auto FramebufferCollectionConfig::attachmentMap(attachments_span_t attachments) -> attachment_map_t {
    attachment_map_t map{};

    for(size_t i = 0; i < attachments.size(); i++)
        map[attachments[i]].push_back(i);

    return map;
}
size_t FramebufferCollectionConfig::minSize(attachments_span_t attachments, size_t max_size) {
    std::unordered_map<AttachmentCollection const*, size_t> map{};
    for(auto const a : attachments)
        ++map[a];

    auto const attachmentMin = std::ranges::min(map | std::views::transform([](auto const& pair) {
        auto const& [attachment, count] = pair;
        return attachment->size() / count;
    }));

    size_t const min = std::min(max_size, attachmentMin);

    CTH_CRITICAL(min == 0, "collection must have size > 0") {}
    return min;
}
FramebufferCollectionConfig::views_t FramebufferCollectionConfig::readViews(attachment_map_t const& map, size_t size, size_t attachments) {
    std::vector<ImageView const*> views{attachments * size};
    std::mdspan const span{views.data(), size, attachments};

    for(size_t frameIndex = 0; frameIndex < size; frameIndex++)
        for(auto const& [attachment, indices] : map)
            for(size_t reuse = 0; reuse < indices.size(); reuse++)
                span[frameIndex, indices[reuse]] = &attachment->view(reuse * size + frameIndex);

    return views;
}
}
