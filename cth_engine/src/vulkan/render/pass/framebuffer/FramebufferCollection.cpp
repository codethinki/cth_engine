#include "FramebufferCollection.hpp"

#include "Framebuffer.hpp"

#include "cth/numeric.hpp"

#include "src/vulkan/render/pass/attachment/AttachmentCollection.hpp"

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

Framebuffer const& FramebufferCollection::get(size_t pulse_index) const {
    CTH_CRITICAL(!cth::num::in(pulse_index, 0, _config.framebuffers), "index out of bounds") {}

    return _framebuffers[pulse_index];
}
VkFramebuffer FramebufferCollection::vkGet(size_t pulse_index) const { return get(pulse_index).get(); }


}
