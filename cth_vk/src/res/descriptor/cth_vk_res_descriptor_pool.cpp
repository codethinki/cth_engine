module;
#include <cth/io/io_log.hpp>
module cth.vk.res.descriptor.pool;

import cth.vk.exception;

import cth.io.log;

//DescriptorPool

namespace cth::vk {

DescriptorPool::DescriptorPool(Core const& core, Builder const& builder) : _core{&core}, _setCount{builder.setCount()} {
    auto const& maxSets = builder._maxDescriptorSets;

    initSetEntries(maxSets);
    create();
    allocSets();
}

DescriptorPool::~DescriptorPool() {
    if(_handle == VK_NULL_HANDLE) return;
    _core->functions()->vkDestroyDescriptorPool(_core->vkDevice(), _handle.get(), nullptr);

    log::msg("destroyed descriptor pool");
}

VkDescriptorSet DescriptorPool::borrowSet(DescriptorSetLayout const& layout) {
    CTH_CRITICAL(!_availableSets.contains(&layout), "layout not contained in pool") {}

    auto& layoutSets = _availableSets.at(&layout);
    auto set = *layoutSets.begin();
    layoutSets.erase(layoutSets.begin());

    CTH_CRITICAL(_dispatchedSets.contains(set), "dispatched sets cannot contain the current set (BUG)") {}
    _dispatchedSets.emplace(set, &layout);

    return set;
}
void DescriptorPool::returnSet(VkDescriptorSet set) {
    CTH_CRITICAL(!_dispatchedSets.contains(set), "set was not dispatched or pool was reset") {}
    auto const layout = _dispatchedSets.at(set);
    _dispatchedSets.erase(set);

    CTH_CRITICAL(std::ranges::contains(_availableSets[layout], set), "set was not dispatched (BUG)") {}
}

void DescriptorPool::reset() {
    CTH_WARN(!_dispatchedSets.empty(), "not all sets were returned but pool was reset") {}

    VkResult const resetResult = _core->functions()->vkResetDescriptorPool(_core->vkDevice(), _handle.get(), 0);
    CTH_STABLE_ERR(resetResult != VK_SUCCESS, "vk: descriptor pool reset failed") {}

    for(auto& available : _availableSets | std::views::values)
        std::ranges::fill(available, VK_NULL_HANDLE);

    allocSets();
}


std::vector<VkDescriptorPoolSize> DescriptorPool::calcPoolSizes() {
    std::unordered_map<VkDescriptorType, uint32_t> maxDescriptorUses{};

    for(auto const& [layout, entry] : _availableSets) {
        auto const& bindings = layout->bindingsVec();
        auto const uses = static_cast<uint32_t>(entry.size());

        for(auto& binding : bindings) maxDescriptorUses[binding.descriptorType] += binding.descriptorCount * uses;
    }


    std::vector<VkDescriptorPoolSize> poolSizes{maxDescriptorUses.size()};
    std::ranges::transform(maxDescriptorUses, poolSizes.begin(), [](auto const& pair) { return VkDescriptorPoolSize{pair.first, pair.second}; });

    return poolSizes;
}
void DescriptorPool::initSetEntries(std::map<DescriptorSetLayout const*, size_t> max_sets) {
    for(auto const& [layout, sets] : max_sets)
        _availableSets.emplace(layout, sets);
}

void DescriptorPool::create() {
    auto const poolSizes = calcPoolSizes();

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pPoolSizes = poolSizes.data();
    createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    createInfo.maxSets = _setCount;

    VkDescriptorPool ptr = VK_NULL_HANDLE;

    VkResult const createResult = _core->functions()->vkCreateDescriptorPool(_core->vkDevice(), &createInfo, nullptr, &ptr);
    CTH_STABLE_ERR(createResult != VK_SUCCESS, "vk: failed to create descriptor pool")
        throw cth::vk::result_exception(createResult, details->exception());

    _handle = ptr;

    log::msg("created descriptor pool");
}

void DescriptorPool::allocSets() {

    std::vector<VkDescriptorSet> vkSets(_setCount);

    std::vector<VkDescriptorSetLayout> vkLayouts{};
    vkLayouts.reserve(_setCount);

    for(auto const& [layout, sets] : _availableSets)
        std::ranges::fill_n(std::back_inserter(vkLayouts), sets.size(), layout->get());


    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _handle.get();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(vkSets.size());
    allocInfo.pSetLayouts = vkLayouts.data();

    VkResult const allocResult = _core->functions()->vkAllocateDescriptorSets(_core->vkDevice(), &allocInfo, vkSets.data());

    CTH_STABLE_ERR(allocResult != VK_SUCCESS, "vk: failed to allocate descriptor sets")
        throw cth::vk::result_exception(allocResult, details->exception());

    auto it = vkSets.begin();
    for(auto& sets : _availableSets | std::views::values)
        for(auto& set : sets) set = *it++;
}
}

//Builder
//TEMP left off here fix this
namespace cth::vk {
void DescriptorPool::Builder::addLayout(DescriptorSetLayout const& layout, uint32_t alloc_count) {
    CTH_WARN(alloc_count == 0, "alloc_count should be > 0") {}

    _maxDescriptorSets[&layout] += alloc_count;
}
void DescriptorPool::Builder::addLayouts(std::map<DescriptorSetLayout const*, uint32_t> const& set_allocations) {
    std::ranges::for_each(set_allocations, [this](auto const& pair) {
        CTH_CRITICAL(pair.first == nullptr, "layout must not be nullptr") {}
        this->addLayout(*pair.first, pair.second);
    });
}
void DescriptorPool::Builder::removeLayout(DescriptorSetLayout const& layout, size_t amount) {
    CTH_WARN(amount == 0, "alloc_count should be > 0") {}
    auto const pLayout = &layout;

    CTH_CRITICAL(!_maxDescriptorSets.contains(pLayout), "builder does not contain layout") {}

    if(amount >= _maxDescriptorSets[pLayout]) _maxDescriptorSets.erase(pLayout);
    else _maxDescriptorSets[pLayout] -= amount;
}
void DescriptorPool::Builder::removeLayouts(std::map<DescriptorSetLayout const*, uint32_t> const& set_allocations) {
    std::ranges::for_each(set_allocations, [this](auto const& pair) {
        CTH_CRITICAL(pair.first == nullptr, "layout must not be nullptr") {}
        this->removeLayout(pair.first, pair.second);
    });
}

} // namespace cth
