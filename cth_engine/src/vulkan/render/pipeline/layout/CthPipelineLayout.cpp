#include "CthPipelineLayout.hpp"

#include "CthDescriptorSetLayout.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthPhysicalDevice.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"



//PipelineLayout

namespace cth::vk {
PipelineLayout::PipelineLayout(cth::not_null<Core const*> core, Builder const& builder) : _core(core),
    _setLayouts(builder.build(core->physicalDevice()->limits().maxBoundDescriptorSets)) { create(); }
PipelineLayout::~PipelineLayout() {
    vkDestroyPipelineLayout(_core->vkDevice(), _vkLayout, nullptr);
    log::msg("destroyed pipeline-layout");
}

void PipelineLayout::create() {
    std::vector<VkDescriptorSetLayout> vkLayouts(_setLayouts.size());
    std::ranges::transform(_setLayouts, vkLayouts.begin(), [](DescriptorSetLayout const* layout) { return layout->get(); });

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(vkLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkLayouts.data();

    VkResult const result = vkCreatePipelineLayout(_core->vkDevice(), &pipelineLayoutInfo, nullptr, &_vkLayout);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create pipeline-layout")
        throw vk::result_exception(result, details->exception());

    log::msg("created pipeline-layout");
}



}


//Builder

namespace cth::vk {
PipelineLayout::Builder& PipelineLayout::Builder::addSetLayouts(std::span<DescriptorSetLayout* const> layouts, uint32_t location_offset) {
    CTH_WARN(layouts.empty(), "layouts vector empty") {}


    for(auto const layout : layouts) addSetLayout(layout, location_offset++);

    return *this;
}
PipelineLayout::Builder& PipelineLayout::Builder::addSetLayout(DescriptorSetLayout* layout, uint32_t location) {
    CTH_WARN(layout == nullptr, "empty layout provided") {}

    auto const keys = _setLayouts | std::views::keys;
    bool const result = std::ranges::any_of(keys, [location](uint32_t key) { return key == location; });
    CTH_ERR(result, "location already used") {
        details->add("location: {}", location);
        throw details->exception();
    }

    _setLayouts.emplace_back(location, layout);

    return *this;
}
PipelineLayout::Builder& PipelineLayout::Builder::removeSetLayout(uint32_t location) {
    CTH_ERR(location >= _setLayouts.size(), "location out of range") throw details->exception();

    if(location == _setLayouts.size() - 1) _setLayouts.pop_back();


    return *this;
}

PipelineLayout::Builder::Builder(std::span<DescriptorSetLayout*> layouts) { addSetLayouts(layouts); }

std::vector<DescriptorSetLayout*> PipelineLayout::Builder::build(uint32_t max_bound_descriptor_sets) const {
    std::vector<DescriptorSetLayout*> result(_setLayouts.size());

    CTH_STABLE_ERR(_setLayouts.size() > max_bound_descriptor_sets, "device limits exceeded, too many locations") {
        details->add("specified: {0}, max: {1}", result.size(), max_bound_descriptor_sets);
        throw details->exception();
    }

    for(auto const& [location, layout] : _setLayouts) result[location] = layout;


    return result;
}

} // namespace cth
