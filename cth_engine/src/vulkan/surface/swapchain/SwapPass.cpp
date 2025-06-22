#include "SwapPass.hpp"

#include "src/vulkan/render/pass/CthRenderPass.hpp"

namespace cth::vk {

SwapPass::SwapPass(Core const& core, BasicSwapchain const& swapchain, RenderPass const& render_pass) :
    _core{&core},
    _swapchain{&swapchain},
    _renderPass{&render_pass} {}
SwapPass::SwapPass(Core const& core, BasicSwapchain const& swapchain, RenderPass const& render_pass, create_t) :
    SwapPass{core, swapchain, render_pass} { create(); }


void SwapPass::create() {
    optDestroy();

}

bool SwapPass::created() const { return _renderPass->created(); }
}
