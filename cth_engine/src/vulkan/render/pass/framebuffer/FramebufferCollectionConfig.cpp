#include "FramebufferCollectionConfig.hpp"

#include "../attachment/AttachmentCollection.hpp"

#include "src/vulkan/render/pass/AttachmentCollection.hpp"

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
    size_t const attachments = std::ranges::fold_left(collections, 0uz, [](size_t sum, AttachmentCollection const* ptr) {
        return sum + ptr->indices().size();
    });


    std::vector<ImageView const*> views{framebuffers * attachments};
    std::mdspan const span{views.data(), framebuffers, attachments};

    for(size_t frameIndex = 0; frameIndex < framebuffers; frameIndex++)
        for(auto const& collection : collections) {
            auto indices = collection->indices();
            for(size_t i = 0; i < indices.size(); i++)
                span[frameIndex, indices[i]] = &collection->view(frameIndex, i);
        }
    return views;
}
}
