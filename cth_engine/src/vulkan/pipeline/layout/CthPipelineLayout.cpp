#include "CthPipelineLayout.hpp"

#include "CthDescriptorSetLayout.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <cth/cth_log.hpp>



//PipelineLayout

namespace cth {
    void PipelineLayout::create() {
        vector<VkDescriptorSetLayout> vkLayouts(setLayouts.size());
        ranges::transform(setLayouts, vkLayouts.begin(), [](const DescriptorSetLayout* layout) { return layout->get(); });

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(vkLayouts.size());
        pipelineLayoutInfo.pSetLayouts = vkLayouts.data();

        const VkResult result = vkCreatePipelineLayout(device->get(), &pipelineLayoutInfo, nullptr, &vkLayout);

        CTH_STABLE_ERR(result != VK_SUCCESS, "Vk: failed to create pipeline layout")
            throw except::vk_result_exception(result, details->exception());

        log::msg("created pipeline layout");
    }


    PipelineLayout::PipelineLayout(Device* device, const Builder& builder) : device(device), setLayouts(builder.build(device)) {
        create();
    }
    PipelineLayout::~PipelineLayout() {
        vkDestroyPipelineLayout(device->get(), vkLayout, nullptr);
        log::msg("destroyed pipeline layout");
    }
}


//Builder

namespace cth {
PipelineLayout::Builder& PipelineLayout::Builder::addSetLayouts(const vector<DescriptorSetLayout*>& layouts, uint32_t location_offset) {
    CTH_WARN(layouts.empty(), "layouts vector empty");


    for(const auto layout : layouts) addSetLayout(layout, location_offset++);

    return *this;
}
PipelineLayout::Builder& PipelineLayout::Builder::addSetLayout(DescriptorSetLayout* layout, const uint32_t location) {
    CTH_WARN(layout == nullptr, "empty layout provided");

    const auto keys = setLayouts | views::keys;
    const bool result = ranges::any_of(keys, [location](const uint32_t key) { return key == location; });
    CTH_ERR(result, "location already used") {
        details->add("location: {}", location);
        throw details->exception();
    }

    setLayouts.emplace_back(location, layout);

    return *this;
}
PipelineLayout::Builder& PipelineLayout::Builder::removeSetLayout(const uint32_t location) {
    CTH_ERR(location >= setLayouts.size(), "location out of range") throw details->exception();

    if(location == setLayouts.size() - 1) setLayouts.pop_back();


    return *this;
}

PipelineLayout::Builder::Builder(const vector<DescriptorSetLayout*>& layouts) { addSetLayouts(layouts); }

vector<DescriptorSetLayout*> PipelineLayout::Builder::build(Device* device) const {
    vector<DescriptorSetLayout*> result(setLayouts.size());

    CTH_STABLE_ERR(setLayouts.size() > device->limits().maxBoundDescriptorSets, "device limits exceeded, too many locations") {
        details->add("specified: {0}, max: {1}", result.size(), device->limits().maxBoundDescriptorSets);
        throw details->exception();
    }

    for(const auto& [location, layout] : setLayouts) result[location] = layout;


    return result;
}

} // namespace cth


