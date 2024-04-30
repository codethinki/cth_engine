#include "CthDescriptorPool.hpp"

#include "CthDescriptorSet.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/render/pipeline/layout/CthDescriptorSetLayout.hpp"
#include "vulkan/utility/CthVkUtils.hpp"


#include <cth/cth_log.hpp>

#include <algorithm>
#include <iterator>



//DescriptorPool

namespace cth {

DescriptorPool::DescriptorPool(Device* device, const Builder& builder) : device(device) {
    initSetEntries(builder);
    create();
    allocSets();
}

DescriptorPool::~DescriptorPool() {
    if(vkPool == VK_NULL_HANDLE) return;
    vkDestroyDescriptorPool(device->get(), vkPool, nullptr);

    log::msg("destroyed descriptor pool");
}

void DescriptorPool::writeSets(const vector<DescriptorSet*>& sets) {
    CTH_WARN(sets.empty(), "sets vector empty") throw details->exception();

    vector<VkWriteDescriptorSet> writes{};

    ranges::for_each(sets, [this, &writes](DescriptorSet* set) {
        CTH_ERR(set == nullptr, "set ptr invalid") throw details->exception();
        CTH_ERR(set->written() || (set->pool != nullptr && set->pool != this), "set already registered in other pool") throw details->exception();

        set->alloc(allocatedSets[set->layout].newVkSet(), this);

        const auto setWrites = set->writes();

        writes.insert(writes.end(), setWrites.begin(), setWrites.end());

        descriptorSets.insert(set);
    });


    vkUpdateDescriptorSets(device->get(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void DescriptorPool::reset() {
    const VkResult resetResult = vkResetDescriptorPool(device->get(), vkPool, 0);


    ranges::for_each(descriptorSets, [](DescriptorSet* set) { set->deallocate(); });

    descriptorSets.clear();

    CTH_STABLE_ERR(resetResult != VK_SUCCESS, "vk: descriptor pool reset failed");
}


vector<VkDescriptorPoolSize> DescriptorPool::calcPoolSizes() {
    unordered_map<VkDescriptorType, uint32_t> maxDescriptorUses{};

    for(const auto& [layout, entry] : allocatedSets) {
        const auto& bindings = layout->bindingsVec();
        const auto uses = static_cast<uint32_t>(entry.size());

        for(auto& binding : bindings) maxDescriptorUses[binding.descriptorType] += binding.descriptorCount * uses;
    }


    vector<VkDescriptorPoolSize> poolSizes{maxDescriptorUses.size()};
    ranges::transform(maxDescriptorUses, poolSizes.begin(), [](const auto& pair) { return VkDescriptorPoolSize{pair.first, pair.second}; });

    return poolSizes;
}
void DescriptorPool::initSetEntries(const Builder& builder) {
    for(auto& [layout, count] : builder.maxDescriptorSets) {
        vkSets.resize(vkSets.size() + count);
        allocatedSets[layout].span = span{&vkSets[vkSets.size() - count], count};
    }
}

void DescriptorPool::create() {
    const vector<VkDescriptorPoolSize> poolSizes = calcPoolSizes();

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.maxSets = static_cast<uint32_t>(vkSets.size());

    const VkResult createResult = vkCreateDescriptorPool(device->get(), &createInfo, nullptr, &vkPool);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "vk: failed to create descriptor pool")
        throw cth::except::vk_result_exception(createResult, details->exception());

    log::msg("created descriptor pool");
}

void DescriptorPool::allocSets() {
    vector<VkDescriptorSetLayout> vkLayouts{};
    vkLayouts.reserve(vkSets.size());

    for(const auto& [layout, entry] : allocatedSets)
        ranges::fill_n(std::back_inserter(vkLayouts), entry.size(), layout->get());


    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vkPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(vkSets.size());
    allocInfo.pSetLayouts = vkLayouts.data();

    const VkResult allocResult = vkAllocateDescriptorSets(device->get(), &allocInfo, vkSets.data());

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "vk: failed to allocate descriptor sets")
        throw cth::except::vk_result_exception(allocResult, details->exception());
}

void DescriptorPool::descriptorSetDestroyed(DescriptorSet* set) {
    CTH_WARN(set == nullptr, "set ptr invalid");
    CTH_WARN(!descriptorSets.contains(set), "set not present in pool");

    descriptorSets.erase(set);
}
} // namespace cth

//Builder

namespace cth {
void DescriptorPool::Builder::addLayout(DescriptorSetLayout* layout, const uint32_t alloc_count) {
    CTH_ERR(layout == nullptr, "layout ptr invalid") throw details->exception();
    CTH_WARN(alloc_count == 0, "alloc_count should be > 0");

    maxDescriptorSets[layout] += alloc_count;
}
void DescriptorPool::Builder::addLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations) {
    ranges::for_each(set_allocations, [this](const auto& pair) { this->addLayout(pair.first, pair.second); });
}
void DescriptorPool::Builder::removeLayout(DescriptorSetLayout* layout, const size_t amount) {
    CTH_ERR(layout == nullptr, "layout ptr invalid") throw details->exception();
    CTH_WARN(amount == 0, "alloc_count should be > 0");
    CTH_ERR(!maxDescriptorSets.contains(layout), "builder does not contain layout") throw details->exception();

    if(amount >= maxDescriptorSets[layout]) maxDescriptorSets.erase(layout);
    else maxDescriptorSets[layout] -= amount;
}
void DescriptorPool::Builder::removeLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations) {
    ranges::for_each(set_allocations, [this](const auto& pair) { this->removeLayout(pair.first, pair.second); });
}

} // namespace cth
