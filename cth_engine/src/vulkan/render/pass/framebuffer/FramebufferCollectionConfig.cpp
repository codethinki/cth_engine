#include "FramebufferCollectionConfig.hpp"

#include "../attachment/AttachmentCollection.hpp"

#include "src/vulkan/render/pass/attachment/AttachmentCollection.hpp"

namespace cth::vk {

FramebufferCollectionConfig FramebufferCollectionConfig::Attachments(attachments_span_t collections, size_t max_framebuffers) {
    auto const framebuffers = evalFramebufferCount(collections, max_framebuffers);

    return {
        .imageViews = readViews(collections, framebuffers),
        .framebuffers = collections.size(),
    };
}

size_t FramebufferCollectionConfig::evalFramebufferCount(attachments_span_t collections, size_t max) {
    return std::min(
        max,
        std::ranges::min(collections | std::views::transform([](AttachmentCollection const* ptr) { return ptr->per_attachment_size(); }))
    );
}
FramebufferCollectionConfig::views_t FramebufferCollectionConfig::readViews(attachments_span_t const& collections, size_t framebuffers) {

    auto const attachments = 1 + std::ranges::max(
        collections | std::views::transform([](auto const* collection) { return collection->indices(); }) | std::views::join
    );

    std::vector<ImageView const*> views{framebuffers * attachments};
    std::mdspan const span(views.data(), framebuffers, attachments);

    for(size_t fbIndex = 0; fbIndex < framebuffers; fbIndex++)
        for(auto const& collection : collections) {
            auto indices = collection->indices();
            for(size_t i = 0; i < indices.size(); i++)
                span[fbIndex, indices[i]] = &collection->view(fbIndex, i);
        }
    return views;
}
}
