#include "vk/render/pass/framebuffer/FramebufferCollection.hpp"

#include "vk/render/pass/attachment/AttachmentCollection.hpp"
#include "vk/render/pass/framebuffer/Framebuffer.hpp"

#include <cth/numeric.hpp>


namespace cth::vk {

FramebufferCollection::FramebufferCollection(Core const& core, RenderPass const& render_pass, Config config) : _core{&core},
    _renderPass{&render_pass}, _config{std::move(config)} {
    Config::debug_check(_config);
    init();
}



FramebufferCollection::FramebufferCollection(Core const& core, RenderPass const& render_pass, Config const& config,
    VkExtent2D framebuffer_extent) : FramebufferCollection{core, render_pass, config} { FramebufferCollection::create(framebuffer_extent); }
FramebufferCollection::~FramebufferCollection() { optDestroy(); }

void FramebufferCollection::create(VkExtent2D framebuffer_extent) {
    optDestroy();

    for(auto& framebuffer : _framebuffers) framebuffer.create(framebuffer_extent);
}

void FramebufferCollection::destroy() {
    CTH_CRITICAL(!created(), "must be created to destroy") {}

    for(auto& framebuffer : _framebuffers) framebuffer.destroy();

    reset();
}
void FramebufferCollection::reconfigure(Config config) {
    _framebuffers.clear();
    _config = std::move(config);
    init();
}

void FramebufferCollection::init() {
    _framebuffers.reserve(_config.framebuffers);

    for(auto const& imageViews : _config.imageViews | cth::views::split_into(_config.framebuffers))
        _framebuffers.emplace_back(*_core, *_renderPass, imageViews);
}
void FramebufferCollection::reset() {}

bool FramebufferCollection::created() const { return _framebuffers[0].created(); }

Framebuffer const& FramebufferCollection::get(size_t index) const {
    CTH_CRITICAL(!cth::num::in(index, 0, _config.framebuffers), "index out of bounds") {}

    return _framebuffers[index];
}
VkFramebuffer FramebufferCollection::vkGet(size_t index) const { return get(index).get(); }


}
