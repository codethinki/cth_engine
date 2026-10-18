#include "cvk/data/descriptor/descriptor_pool.hpp"

#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"


namespace cvk {
DescriptorPool::DescriptorPool(Core const& core, Config config) : _handle{core}, _config{std::move(config)} {
    Core::debug_check(core);
    Config::debug_check(_config);
}

DescriptorPool::DescriptorPool(Core const& core, Config config, State const& state) : DescriptorPool{
    core,
    std::move(config)
} { wrap(state); }

DescriptorPool::DescriptorPool(Core const& core, Config config, create_t) : DescriptorPool{core, std::move(config)} {
    create();
}

void DescriptorPool::wrap(State const& state) {
    optDestroy();

    _handle.reset(state.vkPool.get(), core());
}

void DescriptorPool::create() {
    optDestroy();

    auto const info = _config.createInfo();

    VkDescriptorPool ptr = VK_NULL_HANDLE;
    auto const result = core().functions()->vkCreateDescriptorPool(core().vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create descriptor pool") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(ptr, core());
}

void DescriptorPool::destroy() {
    debug_check(*this);
    CTH_CRITICAL(!unused(), "descriptor sets of the pool must be destroyed first") {
        details->add("descriptor sets: {}", _sets);
    }

    _handle.reset();

    forget();
}

DescriptorPool::State DescriptorPool::release() {
    debug_check(*this);
    CTH_CRITICAL(!unused(), "descriptor sets of the pool must be destroyed first") {
        details->add("descriptor sets: {}", _sets);
    }

    State const state{_handle.release()};
    forget();
    return state;
}

void DescriptorPool::reset() const {
    debug_check(*this);

    auto const result = core().functions()->vkResetDescriptorPool(core().vkDevice(), get(), 0);
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to reset descriptor pool") {}

    _sets = 0;
    ++_resets;
}

void DescriptorPool::add() const { ++_sets; }

void DescriptorPool::remove() const {
    CTH_CRITICAL(unused(), "the pool counts no descriptor set") {}
    --_sets;
}

void DescriptorPool::destroy(DeviceTable table, VkDescriptorPool vkPool) {
    CTH_WARN(vkPool == VK_NULL_HANDLE, "vkPool invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyDescriptorPool(table.device(), vkPool, nullptr);
}

void DescriptorPool::forget() {}
}
