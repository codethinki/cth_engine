module;
#include "lib/volk.hpp"
export module cth.vk.render.exec.pipeline_layout;

import cth.vk.base.device_table;
import cth.vk.base.core;
import cth.vk.res.descriptor.set_layout;

import cth.ptr;

import std;


//TEMP modernize


export namespace cth::vk {
class PipelineLayout {
public:
    struct Builder;
    /**
    * @throws cth::vk::result_exception data: VkResult of vkCreatePipelineLayout()
    * @throws cth::except::exception reason: device limits exceeded, too many locations specified
    */
    PipelineLayout(Core const& core, Builder const& builder);
    ~PipelineLayout();


    static void destroy(DeviceTable table, VkPipelineLayout vk_layout);

private:
    void create();
    void optDestroy() { if(created()) destroy(); }
    void destroy();
    void reset();


    cth::not_null<Core const*> _core;
    cth::move_ptr<VkPipelineLayout_T> _handle = VK_NULL_HANDLE;
    std::vector<DescriptorSetLayout*> _setLayouts{};

public:
    [[nodiscard]] auto created() const { return _handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkPipelineLayout get() const { return _handle.get(); }



    PipelineLayout(PipelineLayout const& other) = delete;
    PipelineLayout(PipelineLayout&& other) = delete;
    PipelineLayout& operator=(PipelineLayout const& other) = delete;
    PipelineLayout& operator=(PipelineLayout&& other) = delete;
};

}

//Builder

export namespace cth::vk {
struct PipelineLayout::Builder {
    Builder() = default;
    explicit Builder(std::span<DescriptorSetLayout*> layouts);

    Builder& addSetLayouts(std::span<DescriptorSetLayout* const> layouts, uint32_t location_offset = 0);
    Builder& addSetLayout(DescriptorSetLayout* layout, uint32_t location);
    Builder& removeSetLayout(uint32_t location);

private:
    /**
     * @throws cth::except::exception reason: device limits exceeded, too many locations specified
     */
    [[nodiscard]] std::vector<DescriptorSetLayout*> build(uint32_t max_bound_descriptor_sets) const;

    std::vector<std::pair<uint32_t, DescriptorSetLayout*>> _setLayouts{};

    friend PipelineLayout;
};
}
