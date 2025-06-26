#include "FramebufferCollection.hpp"

#include "Framebuffer.hpp"

#include "cth/numeric.hpp"

#include "src/vulkan/render/pass/AttachmentCollection.hpp"

namespace cth::vk {

FramebufferCollection::FramebufferCollection(Core const& core, RenderPass const& render_pass, Config const& config) :
    _core{&core},
    _renderPass{&render_pass},
    _size{config.imageViews.size() / config.framebufferSize} {
    Config::debug_check(config);
    init();
}

FramebufferCollection::FramebufferCollection(Core const& core, RenderPass const& render_pass, Config const& config, VkExtent2D framebuffer_extent) :
    FramebufferCollection{core, render_pass, config} { create(framebuffer_extent); }

FramebufferCollection::~FramebufferCollection() { optDestroy(); }


void FramebufferCollection::create(VkExtent2D framebuffer_extent) {
    optDestroy();

    for(auto& framebuffer : _framebuffers)
        framebuffer.create(framebuffer_extent);
}


void FramebufferCollection::destroy() {
    debug_check(*this);

    for(auto& buffer : _framebuffers) buffer.destroy();

    reset();
}


void FramebufferCollection::init() {
    _framebuffers.reserve(_size);

    for(auto const& rng : _views | std::views::chunk(_views.size() / _size))
        _framebuffers.emplace_back(*_core, *_renderPass, rng);
}
void FramebufferCollection::reset() {}

bool FramebufferCollection::created() const { return _framebuffers[0].created(); }

Framebuffer const& FramebufferCollection::get(size_t pulse_index) const {
    CTH_CRITICAL(!cth::num::in(pulse_index, 0, _size), "index out of bounds") {}

    return _framebuffers[pulse_index];
}
VkFramebuffer FramebufferCollection::vkGet(size_t pulse_index) const { return get(pulse_index).get(); }


}
