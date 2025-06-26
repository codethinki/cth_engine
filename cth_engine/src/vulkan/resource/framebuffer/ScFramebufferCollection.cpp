#include "ScFramebufferCollection.hpp"

#include "cth/numeric.hpp"

#include "src/vulkan/render/pass/AttachmentCollection.hpp"
#include "src/vulkan/surface/swapchain/CthBasicSwapchain.hpp"


namespace cth::vk {

ScFramebufferCollection::ScFramebufferCollection(Core const& core, BasicSwapchain const& swapchain,
    RenderPass const& render_pass, FramebufferCollectionConfig const& config) :
    FramebufferCollection{core, render_pass, addSwapchainAttachments(swapchain, config)},
    _swapchain{&swapchain} {}


auto ScFramebufferCollection::addSwapchainAttachments(BasicSwapchain const& swapchain,
    FramebufferCollectionConfig const& config) -> FramebufferCollectionConfig {
    Config::debug_check(config);

    auto const& attachments = swapchain.resolveAttachments();

    auto view = config.imageViews | std::views::chunk(config.framebufferSize);


    auto const newFramebufferSize = config.framebufferSize + 1;
    auto const framebuffers = view.size();

    std::vector<ImageView const*> views{newFramebufferSize * framebuffers * swapchain.size()};
    std::mdspan const span{views.data(), swapchain.size(), framebuffers, newFramebufferSize};


    for(size_t imageIndex = 0; imageIndex < swapchain.size(); imageIndex++)
        for(size_t i = 0; i < framebuffers; i++) {
            span[imageIndex, i, 0] = &attachments->view(imageIndex);
            std::memcpy(
                reinterpret_cast<void*>(&span[imageIndex, i, 1]),
                reinterpret_cast<void const*>(view[static_cast<ptrdiff_t>(i)].data()),
                config.framebufferSize
            );
        }
    return {
        .imageViews = std::move(views),
        .framebufferSize = newFramebufferSize
    };
}
size_t ScFramebufferCollection::size() const {
    return FramebufferCollection::size() / _swapchain->size();
}
Framebuffer const& ScFramebufferCollection::get(size_t pulse_index) const {
    auto const size = this->size();

    CTH_CRITICAL(cth::num::in(pulse_index, 0, size), "index [{}] out of bounds [0, {})", pulse_index, size) {}
    return FramebufferCollection::get(_swapchain->imageIndex(pulse_index) * size + pulse_index);
}
}
