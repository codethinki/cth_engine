#include "CthDescriptorPool.hpp"

#include "CthDescriptorSet.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/render/pipeline/layout/CthDescriptorSetLayout.hpp"
#include "vulkan/utility/CthVkUtils.hpp"



//DescriptorPool

namespace cth::vk {

DescriptorPool::DescriptorPool(const BasicCore* device, const Builder& builder) : _core(device) {
    initSetEntries(builder);
    create();
    allocSets();
}

DescriptorPool::~DescriptorPool() {
    if(_handle == VK_NULL_HANDLE) return;
    vkDestroyDescriptorPool(_core->vkDevice(), _handle.get(), nullptr);

    log::msg("destroyed descriptor pool");
}

void DescriptorPool::writeSets(const std::vector<DescriptorSet*>& sets) {
    CTH_WARN(sets.empty(), "sets vector empty") throw details->exception();

    std::vector<VkWriteDescriptorSet> writes{};

    std::ranges::for_each(sets, [this, &writes](DescriptorSet* set) {
        CTH_ERR(set == nullptr, "set ptr invalid") throw details->exception();
        CTH_ERR(set->written() || (set->_pool != nullptr && set->_pool != this), "set already registered in other pool") throw details->exception();

        set->alloc(_allocatedSets[set->_layout].newVkSet(), this);

        const auto setWrites = set->writes();

        writes.insert(writes.end(), setWrites.begin(), setWrites.end());

        _descriptorSets.insert(set);
    });


    vkUpdateDescriptorSets(_core->vkDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void DescriptorPool::reset() {
    const VkResult resetResult = vkResetDescriptorPool(_core->vkDevice(), _handle.get(), 0);


    std::ranges::for_each(_descriptorSets, [](DescriptorSet* set) { set->deallocate(); });

    _descriptorSets.clear();

    CTH_STABLE_ERR(resetResult != VK_SUCCESS, "vk: descriptor pool reset failed") {}
}


std::vector<VkDescriptorPoolSize> DescriptorPool::calcPoolSizes() {
    std::unordered_map<VkDescriptorType, uint32_t> maxDescriptorUses{};

    for(const auto& [layout, entry] : _allocatedSets) {
        const auto& bindings = layout->bindingsVec();
        const auto uses = static_cast<uint32_t>(entry.size());

        for(auto& binding : bindings) maxDescriptorUses[binding.descriptorType] += binding.descriptorCount * uses;
    }


    std::vector<VkDescriptorPoolSize> poolSizes{maxDescriptorUses.size()};
    std::ranges::transform(maxDescriptorUses, poolSizes.begin(), [](const auto& pair) { return VkDescriptorPoolSize{pair.first, pair.second}; });

    return poolSizes;
}
void DescriptorPool::initSetEntries(const Builder& builder) {
    for(auto& [layout, count] : builder._maxDescriptorSets) {
        _vkSets.resize(_vkSets.size() + count);
        _allocatedSets[layout].span = std::span{&_vkSets[_vkSets.size() - count], count};
    }
}

void DescriptorPool::create() {
    const std::vector<VkDescriptorPoolSize> poolSizes = calcPoolSizes();

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.maxSets = static_cast<uint32_t>(_vkSets.size());

    VkDescriptorPool ptr = VK_NULL_HANDLE;

    const VkResult createResult = vkCreateDescriptorPool(_core->vkDevice(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "vk: failed to create descriptor pool")
        throw cth::except::vk_result_exception(createResult, details->exception());

    _handle = ptr;

    log::msg("created descriptor pool");
}

void DescriptorPool::allocSets() {
    std::vector<VkDescriptorSetLayout> vkLayouts{};
    vkLayouts.reserve(_vkSets.size());

    for(const auto& [layout, entry] : _allocatedSets)
        std::ranges::fill_n(std::back_inserter(vkLayouts), entry.size(), layout->get());


    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _handle.get();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(_vkSets.size());
    allocInfo.pSetLayouts = vkLayouts.data();

    const VkResult allocResult = vkAllocateDescriptorSets(_core->vkDevice(), &allocInfo, _vkSets.data());

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "vk: failed to allocate descriptor sets")
        throw cth::except::vk_result_exception(allocResult, details->exception());
}

void DescriptorPool::returnSet(DescriptorSet* set) {
    CTH_WARN(set == nullptr, "set ptr invalid") {}
    CTH_WARN(!_descriptorSets.contains(set), "set not present in pool") {}

    _descriptorSets.erase(set);
}
} // namespace cth

//Builder

namespace cth::vk {
void DescriptorPool::Builder::addLayout(const DescriptorSetLayout* layout, const uint32_t alloc_count) {
    CTH_ERR(layout == nullptr, "layout ptr invalid") throw details->exception();
    CTH_WARN(alloc_count == 0, "alloc_count should be > 0") {}

    _maxDescriptorSets[layout] += alloc_count;
}
void DescriptorPool::Builder::addLayouts(const std::unordered_map<const DescriptorSetLayout*, uint32_t>& set_allocations) {
    std::ranges::for_each(set_allocations, [this](const auto& pair) { this->addLayout(pair.first, pair.second); });
}
void DescriptorPool::Builder::removeLayout(const DescriptorSetLayout* layout, const size_t amount) {
    CTH_ERR(layout == nullptr, "layout ptr invalid") throw details->exception();
    CTH_WARN(amount == 0, "alloc_count should be > 0") {}
    CTH_ERR(!_maxDescriptorSets.contains(layout), "builder does not contain layout") throw details->exception();

    if(amount >= _maxDescriptorSets[layout]) _maxDescriptorSets.erase(layout);
    else _maxDescriptorSets[layout] -= amount;
}
void DescriptorPool::Builder::removeLayouts(const std::unordered_map<const DescriptorSetLayout*, uint32_t>& set_allocations) {
    std::ranges::for_each(set_allocations, [this](const auto& pair) { this->removeLayout(pair.first, pair.second); });
}

} // namespace cth
