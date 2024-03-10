#include "CthDescriptorPool.hpp"

#include "CthDescriptorSet.hpp"
#include "../../core/CthDevice.hpp"
#include "../../utils/cth_vk_specific_utils.hpp"

#include <iterator>
#include <algorithm>
#include <Windows.h>

#include <cth/cth_log.hpp>

#include "CthDescriptorSetLayout.hpp"

//Builder

namespace cth {
void DescriptorPool::Builder::addLayout(DescriptorSetLayout* layout, uint32_t alloc_count) {
    CTH_ERR(layout == nullptr, "layout ptr invalid") throw details->exception();
    CTH_WARN(alloc_count == 0, "alloc_count should be > 0");

    maxDescriptorSets[layout] += alloc_count;
}
void DescriptorPool::Builder::addLayouts(const unordered_map<DescriptorSetLayout*, uint32_t>& set_allocations) {
    ranges::for_each(set_allocations, [this](const auto& pair) { this->addLayout(pair.first, pair.second); });
}
void DescriptorPool::Builder::removeLayout(DescriptorSetLayout* layout, const uint32_t amount) {
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



//DescriptorPool

namespace cth {


VkResult DescriptorPool::allocSets(const vector<DescriptorSet*>& sets) {
    CTH_ERR(sets.empty(), "sets vector empty") throw details->exception();

    vector<VkDescriptorSetLayout> vkLayouts{sets.size()};
    ranges::transform(sets, vkLayouts.begin(), [](const DescriptorSet* set) { return set->layout->get(); });

    vector<VkDescriptorSet> vkSets{sets.size()};
    ranges::transform(sets, vkSets.begin(), [](const DescriptorSet* set) { return set->get(); });


    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vkPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(sets.size());
    allocInfo.pSetLayouts = vkLayouts.data();

    const VkResult allocResult = vkAllocateDescriptorSets(device->device(), &allocInfo, vkSets.data());


    //TEMP left off here. recode the pool so that the descriptor sets are allocated beforehand and "allocating" is just writing to them so maybe use different descriptor set layouts as "max_sets" and calculate the max_descriptor_uses from that
    vector<VkWriteDescriptorSet> writes{};

    ranges::for_each(sets, [this, &writes](DescriptorSet* set) {
        CTH_ERR(set == nullptr, "set ptr invalid") throw details->exception();
        CTH_ERR(set->allocated() || (set->pool != nullptr && set->pool != this), "set already registered in other pool") throw details->exception();

        set->pool = this;
        set->allocState = true;

        const auto setWrites = set->writes();

        writes.insert(writes.end(), setWrites.begin(), setWrites.end());
    });


}

void DescriptorPool::reset(const bool clear_sets) {
    CTH_WARN(_reset, "descriptor pool already reset");

    const VkResult resetResult = vkResetDescriptorPool(device->device(), vkPool, 0);

    ranges::for_each(descriptorSets, [](DescriptorSet* set) {
        set->allocState = false;

        //TEMP remove this after testing and then also remove the set->allocState flag if resetSet == VK_NULL_HANDLE
        CTH_INFORM(set->vkSet == VK_NULL_HANDLE, "sets are reset to VK_NULL_HANDLE after resetting the pool")

            set->vkSet = VK_NULL_HANDLE;
    });

    if(clear_sets) clear();

    CTH_STABLE_ERR(resetResult != VK_SUCCESS, "vk: descriptor pool reset failed");
}

void DescriptorPool::removeDescriptorSet(DescriptorSet* set) {
    CTH_ERR(set == nullptr, "set ptr invalid")
        throw details->exception();
    CTH_ERR(!descriptorSets.contains(set), "set not present in pool")
        throw details->exception();
    CTH_ERR(set->allocated(), "removing requires unallocated set")
        throw details->exception();

    set->pool = nullptr;
    descriptorSets.erase(set);
}
void DescriptorPool::removeDescriptorSets(const vector<DescriptorSet*>& sets) {
    ranges::for_each(sets, [this](DescriptorSet* set) { removeDescriptorSet(set); });
}
void DescriptorPool::clear() {
    CTH_ERR(!_reset, "requires reset pool") throw details->exception();

    ranges::for_each(descriptorSets, [](DescriptorSet* set) { set->pool = nullptr; });
}


vector<VkDescriptorPoolSize> DescriptorPool::calcPoolSizes() {
    unordered_map<VkDescriptorType, uint32_t> maxDescriptorUses{};
    ranges::for_each(maxDescriptorSets, [&maxDescriptorUses](const std::pair<DescriptorSetLayout*, uint32_t>& pair) {
        const auto& bindings = pair.first->bindings();

        ranges::for_each(bindings, [&maxDescriptorUses](const VkDescriptorSetLayoutBinding& binding) {
            maxDescriptorUses[binding.descriptorType] += binding.descriptorCount;
        });
    });


    vector<VkDescriptorPoolSize> poolSizes{maxDescriptorUses.size()};
    ranges::transform(maxDescriptorUses, poolSizes.begin(), [](const auto& pair) { return VkDescriptorPoolSize{pair.first, pair.second}; });

    return poolSizes;
}

void DescriptorPool::create() {
    const vector<VkDescriptorPoolSize> poolSizes = calcPoolSizes();
    const uint32_t maxSets = ranges::fold_left(maxDescriptorSets, 0, [](uint32_t sum, const auto& pair) { return sum + pair.second; });

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.maxSets = maxSets;

    const VkResult createResult = vkCreateDescriptorPool(device->device(), &createInfo, nullptr, &vkPool);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "vk: failed to create descriptor pool")
        throw cth::except::vk_result_exception(createResult, details->exception());
}

void DescriptorPool::descriptorSetDestroyed(DescriptorSet* set) {
    CTH_ERR(set == nullptr, "set ptr invalid");
    CTH_ERR(!descriptorSets.contains(set), "set not present in pool");
    CTH_ERR(set->allocated(), "destroying requires unallocated set");

    descriptorSets.erase(set);
}
//TEMP remove
//DescriptorPool::DescriptorPool(Device* device, const uint32_t max_allocated_sets,
//    const unordered_map<VkDescriptorType, uint32_t>& max_descriptor_uses) : device(device), maxAllocatedSets(max_allocated_sets) {
//    std::ranges::for_each(max_descriptor_uses, [this](const auto& pair) {
//        const auto [type, count] = pair;
//        maxDescriptorUses[type] = count;
//    });
//
//
//    create();
//}

DescriptorPool::DescriptorPool(Device* device, const Builder& builder) : device(device), maxDescriptorSets{builder.maxDescriptorSets} { create(); }

DescriptorPool::~DescriptorPool() {
    if(vkPool == VK_NULL_HANDLE) return;
    if(!_reset) reset(true);
    vkDestroyDescriptorPool(device->device(), vkPool, nullptr);
}
} // namespace cth
