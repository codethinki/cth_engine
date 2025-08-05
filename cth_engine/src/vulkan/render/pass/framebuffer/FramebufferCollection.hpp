#pragma once
#include "FramebufferCollectionConfig.hpp"

#include "src/interface/render/RenderPulse.hpp"
#include "src/vulkan/utility/cth_constants.hpp"

namespace cth::vk {
class Framebuffer;
class RenderPass;
class AttachmentCollection;
class Core;
struct FramebufferCollectionConfig;

class FramebufferCollection {
public:
    using Config = FramebufferCollectionConfig;

    /**
     * @brief constructs
     */
    FramebufferCollection(Core const& core, RenderPass const& render_pass, Config config);

    /**
     * @brief constructs and creates
     * @details calls:
     *   - @ref FramebufferCollection(Core const&, RenderPass const&, Config const&)
     */
    FramebufferCollection(Core const& core, RenderPass const& render_pass, Config const& config, VkExtent2D framebuffer_extent);

    virtual ~FramebufferCollection();

    /**
     * @brief creates the framebuffers
     * @details calls:
     *  - @ref optDestroy()
     *  - @ref Framebuffer::create()
     *  @attention requires @ref AttachmentCollection::created()
     */
    virtual void create(VkExtent2D framebuffer_extent);

    //TODO void wrap();

    /**
     * @brief calls @ref destroy() if @ref created()
     */
    void optDestroy() { if(created()) destroy(); }


    /**
     * @details calls @ref Framebuffer::destroy()
     * @attention requires @ref created()
     */
    void destroy();

protected:
    void reconfigure(Config config);

private:
    using views_t = Config::views_t;
    void init();
    void reset();

    cth::not_null<Core const*> _core;
    cth::not_null<RenderPass const*> _renderPass;

    Config _config;

    std::vector<Framebuffer> _framebuffers;

public:
    [[nodiscard]] bool created() const;
    [[nodiscard]] virtual size_t size() const { return _config.framebuffers; }


    [[nodiscard]] virtual Framebuffer const& get(size_t pulse_index) const;
    [[nodiscard]] virtual Framebuffer const& get(RenderPulse const& pulse) const { return get(pulse.get()); }
    [[nodiscard]] VkFramebuffer vkGet(size_t pulse_index) const;
    [[nodiscard]] VkFramebuffer vkGet(RenderPulse const& pulse) const { return vkGet(pulse.get()); };

    FramebufferCollection(FramebufferCollection const& other) = delete;
    FramebufferCollection& operator=(FramebufferCollection const& other) = delete;
    FramebufferCollection(FramebufferCollection&& other) noexcept = default;
    FramebufferCollection& operator=(FramebufferCollection&& other) noexcept = default;


    static void debug_check(FramebufferCollection const& collection) {
        CTH_CRITICAL(!collection.created(), "collection must be created") {}
    }
};
}
