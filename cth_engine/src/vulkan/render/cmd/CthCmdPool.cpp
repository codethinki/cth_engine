#include "CthCmdPool.hpp"

#include "CthCmdBuffer.hpp"
#include "vulkan/base/CthCore.hpp"
#include "vulkan/base/CthDevice.hpp"
#include "vulkan/base/queue/CthQueue.hpp"
#include "vulkan/resource/CthDestructionQueue.hpp"
#include "vulkan/utility/cth_vk_exceptions.hpp"

#include <range/v3/view/concat.hpp>


namespace cth::vk {

CmdPool::CmdPool(cth::not_null<Core const*> core, Config const& config) : _core{core}, _flags{config.flags},
    _queueFamilyIndex{config.queueFamilyIndex} {
    Core::debug_check(_core);

    _maxBuffers[BUFFER_TYPE_PRIMARY] = config.maxPrimaryBuffers;
    _maxBuffers[BUFFER_TYPE_SECONDARY] = config.maxSecondaryBuffers;

    for(size_t i = 0; i < _maxBuffers.size(); i++) _buffers[i].resize(_maxBuffers[i]);
}
CmdPool::CmdPool(cth::not_null<Core const*> core, Config const& config, State const& state) : CmdPool{core, config} { wrap(state); }
CmdPool::CmdPool(cth::not_null<Core const*> core, Config const& config, bool create) : CmdPool{core, config} { if(create) this->create(); }


CmdPool::~CmdPool() { optDestroy(); }

void CmdPool::wrap(State const& state) {

    CTH_CRITICAL(
        state.vkPrimaryBuffers.size() != _maxBuffers[BUFFER_TYPE_PRIMARY] || state.vkSecondaryBuffers.size() != _maxBuffers[BUFFER_TYPE_SECONDARY],
        "buffers must match size") {
        details->add("expected primary: {0}, actual primary: {1}", _maxBuffers[BUFFER_TYPE_PRIMARY], state.vkPrimaryBuffers.size());
        details->add("expected secondary: {0}, actual secondary: {1}", _maxBuffers[BUFFER_TYPE_SECONDARY], state.vkSecondaryBuffers.size());
    }

    _handle = state.vkPool.get();

    std::ranges::transform(state.vkPrimaryBuffers, _buffers[BUFFER_TYPE_PRIMARY].begin(), [](auto const& buffer) { return buffer.get(); });
    std::ranges::transform(state.vkSecondaryBuffers, _buffers[BUFFER_TYPE_SECONDARY].begin(), [](auto const& buffer) { return buffer.get(); });
}


void CmdPool::create() {
    optDestroy();
    createPool();
    alloc();
}
CmdPool::State CmdPool::release() {
    debug_check(this);

    State state{
        .vkPool = _handle.get(),
        .vkPrimaryBuffers{std::from_range, _buffers[BUFFER_TYPE_PRIMARY]},
        .vkSecondaryBuffers{std::from_range, _buffers[BUFFER_TYPE_SECONDARY]},
    };
    reset();

    return state;
}


void CmdPool::destroy() {
    debug_check(this);

    std::vector const buffers{std::from_range, std::views::join(_buffers)};
    std::array<DestructionQueue::function_t, 2> const lambdas{
        [vk_device = _core->vkDevice(), vk_pool = _handle.get(), buffers] { CmdBuffer::destroy(vk_device, vk_pool, buffers); },
        [vk_device = _core->vkDevice(), vk_pool = _handle.get()] { destroy(vk_device, vk_pool); },
    };

    auto const queue = _core->destructionQueue();

    if(queue) queue->push(lambdas);
    else for(auto& lambda : lambdas) lambda();

    _handle = VK_NULL_HANDLE; //TEMP call reset here
}
void CmdPool::optDestroy(this auto&& self) { if(self.created()) self.destroy(); }

void CmdPool::destroy(vk::not_null<VkDevice> vk_device, VkCommandPool vk_pool) {
    Device::debug_check_handle(vk_device);

    CTH_WARN(vk_pool == VK_NULL_HANDLE, "pool should not be invalid (VK_NULL_HANDLE)") {}

    vkDestroyCommandPool(vk_device.get(), vk_pool, nullptr);
}



void CmdPool::reset() {
    _handle = VK_NULL_HANDLE;
    for(auto [buffers, maxBuffers] : std::views::zip(_buffers, _maxBuffers)) {
        buffers.clear();
        buffers.resize(maxBuffers);
    }
}

template<cmd_buffer_t T>
VkCommandBuffer CmdPool::newCmdBuffer() {
    auto const index = to_enum<T>();
    auto& buffers = _buffers[index];

    CTH_CRITICAL(buffers.empty(), "too many buffers requested, consider raising the max buffer limit") {}

    auto handle = buffers.back();
    buffers.pop_back();

    return handle;
}

template<cmd_buffer_t T>
void CmdPool::returnCmdBuffer(VkCommandBuffer vk_buffer) {
    auto const index = to_enum<T>();
    auto& buffers = _buffers[index];
    auto const maxBuffers = _maxBuffers[index];

    CTH_CRITICAL(maxBuffers <= std::ranges::size(buffers), "cannot return more than max buffers") {}


    buffers.push_back(vk_buffer);
}


VkCommandPoolCreateInfo CmdPool::createInfo() const {
    return VkCommandPoolCreateInfo{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        _flags,
        _queueFamilyIndex
    };
}

void CmdPool::createPool() {
    auto const info = createInfo();

    VkCommandPool ptr = VK_NULL_HANDLE;
    auto const result = vkCreateCommandPool(_core->vkDevice(), &info, nullptr, &ptr);

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create command pool")
        throw cth::vk::result_exception{result, details->exception()};

    _handle = ptr;
}

void CmdPool::alloc() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _handle.get();

    for(BufferType i = BUFFER_TYPE_PRIMARY; i < BUFFER_TYPES; i = static_cast<BufferType>(i + 1)) {
        auto& buffers = _buffers[i];

        if(buffers.empty()) continue;

        allocInfo.level = to_buffer_level(i);
        allocInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());

        auto const allocResult = vkAllocateCommandBuffers(_core->vkDevice(), &allocInfo, buffers.data());

        CTH_STABLE_ERR(allocResult != VK_SUCCESS, "failed to allocate group({}) command buffers (0 = PRIMARY, 1 = SECONDARY)", static_cast<size_t>(i))
            throw cth::vk::result_exception{allocResult, details->exception()};
    }
}

}

//Config

namespace cth::vk {
CmdPool::Config CmdPool::Config::Default(Queue const& queue, uint32_t max_primary_buffers, uint32_t max_secondary_buffers) {
    return Config{max_primary_buffers, max_secondary_buffers, queue.familyIndex()};
}
}
