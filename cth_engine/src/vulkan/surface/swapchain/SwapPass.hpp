#pragma once
#include "src/vulkan/utility/cth_constants.hpp"

namespace cth::vk {
class Core;
class BasicSwapchain;
class RenderPass;

struct RenderPassConfig;


class SwapPass {
public:
    SwapPass(Core const& core, BasicSwapchain const& swapchain, RenderPass const& render_pass);

    /**
     * @brief constructs and creates
     * @details calls:
     *  - @ref SwapPass(Core const&, BasicSwapchain const&, RenderPass const& render_pass)
     *  - @ref create()
     */
    SwapPass(Core const& core, BasicSwapchain const& swapchain, RenderPass const& render_pass, create_t);

    /**
     * @details calls:
     * @attention requires:
     *  - @ref BasicSwapchain::created()
     *  - @ref RenderPass::created()
     */
    void create();


    /**
     * @attention requires @ref created();
     */
    void destroy();

    /**
     * @brief destroys if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }

private:
    cth::not_null<Core const*> _core;
    cth::not_null<BasicSwapchain const*> _swapchain;
    cth::not_null<RenderPass const*> _renderPass;

public:
    [[nodiscard]] bool created() const;
};

}
