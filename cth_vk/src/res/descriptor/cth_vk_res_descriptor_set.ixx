module;
#include "lib/volk.hpp"
export module cth.vk.res.descriptor.set;

import cth.vk.res.buffer.base;
import cth.vk.res.descriptors.base;
import cth.vk.res.descriptor.pool;
import cth.vk.res.desciptor.set_layout;
import cth.vk.base.core;

import cth.ptr;
import std;

export namespace cth::vk {
class DescriptorSet {
    enum class InfoType {
        BUFFER,
        IMAGE,
        NONE
    };

public:
    struct Config;
    explicit DescriptorSet(Core const& core, DescriptorPool& pool, Config const& config);
    virtual ~DescriptorSet();

private:
    void writeDescriptors();
    void alloc(VkDescriptorSet set, DescriptorPool* pool);
    void deallocate();
    [[nodiscard]] virtual std::vector<VkWriteDescriptorSet> writes();

    void clearDescriptors() { _descriptors.clear(); }

    void copyInfos();

    [[nodiscard]] static InfoType infoType(VkDescriptorType descriptor_type);

    cth::not_null<Core const*> _core;
    cth::not_null<DescriptorPool*> _pool;
    DescriptorSetLayout const* _layout;
    std::vector<std::vector<Descriptor*>> _descriptors{};
    std::vector<VkDescriptorBufferInfo> _bufferInfos{};
    std::vector<VkDescriptorImageInfo> _imageInfos{};

    move_ptr<VkDescriptorSet_T> _handle = VK_NULL_HANDLE;
    bool _written = false;


public:
    [[nodiscard]] VkDescriptorSet get() const { return _handle.get(); }
    [[nodiscard]] bool written() const { return _written; }



    DescriptorSet(DescriptorSet const& other) = delete;
    DescriptorSet(DescriptorSet&& other) = delete;
    DescriptorSet& operator=(DescriptorSet const& other) = delete;
    DescriptorSet& operator=(DescriptorSet&& other) = delete;
};

}

export namespace cth::vk {
struct DescriptorSet::Config {
    explicit Config(DescriptorSetLayout const* layout);

    /**
     *@note in this constructor only one resource per binding can be specified
     */
    explicit Config(DescriptorSetLayout const* layout, std::span<Descriptor* const> descriptors, uint32_t binding_offset = 0);

    Config& addDescriptor(Descriptor* descriptor, uint32_t binding, uint32_t arr_index = 0);
    Config& addDescriptors(std::span<Descriptor* const> binding_descriptors, uint32_t binding, uint32_t arr_first);

    Config& removeDescriptor(uint32_t binding, uint32_t arr_index);
    Config& removeDescriptors(uint32_t binding, uint32_t arr_first, uint32_t count);

private:
    void init(DescriptorSetLayout const* layout);

    DescriptorSetLayout const* _layout;
    std::vector<std::vector<Descriptor*>> _descriptors{};

    friend DescriptorSet;
};
}
