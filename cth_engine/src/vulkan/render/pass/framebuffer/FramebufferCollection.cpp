#include "FramebufferCollection.hpp"

#include "Framebuffer.hpp"

#include "cth/numeric.hpp"

#include "src/vulkan/render/pass/AttachmentCollection.hpp"

namespace cth::vk {

FramebufferCollection::3,0
+FramebufferCollection(Core const& core, RenderPass const& render_pass, Config c
    ,,,,,,,,,,,,,,,,,,,,,,,+3,

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
