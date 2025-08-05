#include "ScFramebufferCollection.hpp"

#include "Framebuffer.hpp"

#include "src/vulkan/render/pass/attachment/AttachmentCollection.hpp"
#include "src/vulkan/surface/swapchain/Swapchain.hpp"
#include "src/vulkan/utility/cth_vk_overloads.hpp"

#include <cth/numeric.hpp>


namespace cth::vk {

ScFramebufferCollection::ScFramebufferCollection(Core const& core, Swapchain const& swapchain,
    RenderPass const& render_pass, FramebufferCollectionConfig const& config) :
    FramebufferCollection{core, render_pass, addSwapchainAttachments(swapchain, config)},
    _swapchain{&swapchain}, _scImages{_swapchain->size()} {}
ScFramebufferCollection::ScFramebufferCollection(Core const& core, Swapchain const& swapchain, RenderPass const& render_pass,
    FramebufferCollectionConfig const& config, create_t) : ScFramebufferCollection{core, swapchain, render_pass, config} {
    create(_swapchain->extent(), false);
}
void ScFramebufferCollection::create() { ScFramebufferCollection::create(_swapchain->extent()); }
void ScFramebufferCollection::create(VkExtent2D framebuffer_extent) { create(framebuffer_extent, true); }
void ScFramebufferCollection::swapchainResized() {
    std::vector<ImageView const*> attachmentViews{};

    for(size_t i = 0; i < size(); i++) {
        auto views = get(0, i).attachments();
        attachmentViews.insert_range(attachmentViews.end(), views);
    }


    auto config = addSwapchainAttachments(
        *_swapchain,
        {
            .imageViews = std::move(attachmentViews),
            .framebuffers = size(),
        }
    );
    _scImages = _swapchain->size();

    reconfigure(std::move(config));
}
void ScFramebufferCollection::create(VkExtent2D extent, bool reconfigure) {
    CTH_WARN(extent != _swapchain->extent(), "swapchain framebuffer collection extent should be equal to swapchain extent") {}

    if(reconfigure) swapchainResized();

    FramebufferCollection::create(extent);
}


auto ScFramebufferCollection::addSwapchainAttachments(Swapchain const& swapchain,
    FramebufferCollectionConfig const& config) -> FramebufferCollectionConfig {
    Config::debug_check_size(config);
    Swapchain::debug_check(swapchain);

    auto const& scAttachments = swapchain.resolveAttachments();
    CTH_CRITICAL(scAttachments->indices().size() > 1, "resolve attachments must only have one index") {}

    auto const scAttachmentIndex = scAttachments->indices()[0];


    auto view = config.imageViews | cth::views::split_into(config.framebuffers);

    auto const newFramebufferSize = std::max(config.framebufferSize(), static_cast<size_t>(scAttachmentIndex) + 1);

    size_t const scSize = swapchain.size();

    std::vector<ImageView const*> views{newFramebufferSize * config.framebuffers * scSize};
    std::mdspan const span{views.data(), scSize, config.framebuffers, newFramebufferSize};


    for(size_t scIndex = 0; scIndex < span.extent(0); scIndex++)
        for(size_t fbIndex = 0; fbIndex < span.extent(1); fbIndex++) {
            for(size_t imIndex = 0; imIndex < view[fbIndex].size(); imIndex++)
                span[scIndex, fbIndex, imIndex] = view[fbIndex][imIndex];

            span[scIndex, fbIndex, scAttachmentIndex] = &scAttachments->view(scIndex);
        }

    Config result{
        .imageViews = std::move(views),
        .framebuffers = config.framebuffers * scSize
    };

    Config::debug_check(result);

    return result;
}
size_t ScFramebufferCollection::size() const {
    auto const size = FramebufferCollection::size();
    CTH_CRITICAL(size % _scImages != 0, "size [{}] must be divisible by swapchain size [{}]", size, _scImages);

    return size / _scImages;
}
Framebuffer const& ScFramebufferCollection::get(size_t pulse_index) const { return get(_swapchain->imageIndex(pulse_index), pulse_index); }
Framebuffer const& ScFramebufferCollection::get(size_t swapchain_image_index, size_t pulse_index) const {
    auto const size = ScFramebufferCollection::size();
    CTH_CRITICAL(!cth::num::in(pulse_index, 0, size), "index [{}] out of bounds [0, {})", pulse_index, size) {}

    CTH_CRITICAL(!cth::num::in(swapchain_image_index, 0, _scImages), "swapchain image index out of bounds [0, {})", _scImages) {}


    return FramebufferCollection::get(swapchain_image_index * size + pulse_index);
}
}
