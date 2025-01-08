#include "CthPipelineLayout.hpp"

#include "CthDescriptorSetLayout.hpp"
#include "src/vulkan/base/CthCore.hpp"
#include "src/vulkan/base/CthDeviceTable.hpp"
#include "src/vulkan/base/CthPhysicalDevice.hpp"
#include "src/vulkan/resource/CthDestructionQueue.hpp"
#include "src/vulkan/utility/cth_vk_exceptions.hpp"



//PipelineLayout

namespace cth::vk {
PipelineLayout::PipelineLayout(cth::not_null<Core const*> core, Builder const& builder) : _core{core},
    _setLayouts{builder.build(core->physicalDevice()->limits().maxBoundDescriptorSets)} { create(); }
PipelineLayout::~PipelineLayout() { optDestroy(); }
void PipelineLayout::destroy(DeviceTable table, VkPipelineLayout vk_layout) {
    CTH_WARN(vk_layout == VK_NULL_HANDLE, "vk_layout should not be invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyPipelineLayout(table.device(), vk_layout, nullptr);

    log::msg("destroyed pipeline-layout"); //TEMP
}

void PipelineLayout::create() {
    std::vector<VkDescriptorSetLayout> vkLayouts(_setLayouts.size());
    std::ranges::transform(_setLayouts, vkLayouts.begin(), [](DescriptorSetLayout const* layout) { return layout->get(); });

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(vkLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkLayouts.data();

    VkPipelineLayout ptr = VK_NULL_HANDLE;
    VkResult const result = _core->functions()->vkCreatePipelineLayout(_core->vkDevice(), &pipelineLayoutInfo, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create pipeline-layout")
        throw vk::result_exception(result, details->exception());
    _handle = ptr;

    log::msg("created pipeline-layout");
}
void PipelineLayout::destroy() {
    CTH_CRITICAL(!created(), "created() required") {}

    auto const lambda = [table = _core->deviceTable(), handle = _handle.get()] { destroy(table, handle); };

    auto const queue = _core->destructionQueue();
    if(queue) queue->push(lambda);
    else lambda();
    reset();
}
void PipelineLayout::reset() { _handle = VK_NULL_HANDLE; }



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

    CTH_CRITICAL(result, "location({}) already used", location) {}

    _setLayouts.emplace_back(location, layout);

    return *this;
}
PipelineLayout::Builder& PipelineLayout::Builder::removeSetLayout(uint32_t location) {
    CTH_CRITICAL(location >= _setLayouts.size(), "location out of range") {}

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
