#include "cvk/data/img/image.hpp"

#include "cvk/base/allocator.hpp"
#include "cvk/base/core.hpp"
#include "cvk/utility/vk_exceptions.hpp"

#include <algorithm>


namespace cvk {
Image::Image(Core const& core, Config const& config) : _handle{core}, _memory{core, config.memory},
    _config{config}, _levelLayouts(config.mipLevels, config.initialLayout) {
    Core::debug_check(core);
    Config::debug_check(config);
}

Image::Image(Core const& core, Config const& config, State const& state) : Image{core, config} { wrap(state); }

Image::Image(Core const& core, Config const& config, VkExtent2D extent) : Image{core, config} { create(extent); }

void Image::wrap(State const& state) {
    CTH_CRITICAL(state.levelLayouts.size() > _config.mipLevels, "more level layouts than mip levels") {}

    optDestroy();

    _handle.reset(state.vkImage.get(), core());
    if(state.memory.has_value()) _memory.wrap(*state.memory);
    _extent = state.extent;
    _levelLayouts.assign(_config.mipLevels, _config.initialLayout);
    std::ranges::copy(state.levelLayouts, _levelLayouts.begin());
}

void Image::create(VkExtent2D extent) {
    optDestroy();

    auto const imageInfo = _config.createInfo(extent);
    auto const allocationInfo = _memory.config().createInfo();

    VkImage vkImage = VK_NULL_HANDLE;
    VmaAllocation vmaAllocation = VK_NULL_HANDLE;
    auto const result = vmaCreateImage(
        core().allocator().get(),
        &imageInfo,
        &allocationInfo,
        &vkImage,
        &vmaAllocation,
        nullptr
    );

    CTH_STABLE_ERR(result != VK_SUCCESS, "failed to create image") {
        forget();
        throw cvk::vk_result_exception{result, details->exception()};
    }

    _handle.reset(vkImage, core());
    _memory.wrap(Memory::State{vmaAllocation});
    _extent = extent;
    _levelLayouts.assign(_config.mipLevels, _config.initialLayout);
}

void Image::destroy() {
    debug_check(*this);

    _handle.reset();
    _memory.optDestroy();

    forget();
}

Image::State Image::release() {
    debug_check(*this);

    State state{
        .vkImage = _handle.release(),
        .extent = _extent,
        .memory = std::nullopt,
        .levelLayouts = std::move(_levelLayouts),
    };
    if(_memory.created()) state.memory = _memory.release();

    forget();
    return state;
}

void Image::destroy(DeviceTable table, VkImage vkImage) {
    CTH_WARN(vkImage == VK_NULL_HANDLE, "vkImage invalid (VK_NULL_HANDLE)") {}

    table->vkDestroyImage(table.device(), vkImage, nullptr);
}

void Image::forget() {
    _extent = {};
    _levelLayouts.assign(_config.mipLevels, _config.initialLayout);
}
}
