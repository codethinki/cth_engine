#include "jvk/sync/resource_states.hpp"

#include "cvk/data/buffer/base_buffer.hpp"
#include "cvk/data/img/image.hpp"

#include <cth/io/log.hpp>


namespace jvk {
using cvk::BarrierScope;
using cvk::BaseBuffer;
using cvk::Image;

namespace {
    /**
     * the write accesses
     */
    constexpr VkAccessFlags2 WRITES = VK_ACCESS_2_SHADER_WRITE_BIT
        | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT
        | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_2_TRANSFER_WRITE_BIT
        | VK_ACCESS_2_HOST_WRITE_BIT
        | VK_ACCESS_2_MEMORY_WRITE_BIT
        | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
        | VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
        | VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;

    [[nodiscard]] constexpr bool writes(VkAccessFlags2 access) { return (access & WRITES) != 0; }

    /**
     * src of a barrier after the scope, only the writes of its access
     */
    [[nodiscard]] constexpr BarrierScope src_scope(BarrierScope const& scope) {
        return BarrierScope{scope.stages, scope.access & WRITES, scope.queueFamilyIndex};
    }

    /**
     * the stages and accesses of both scopes
     */
    [[nodiscard]] constexpr BarrierScope merge(BarrierScope const& scope, BarrierScope const& other) {
        return BarrierScope{scope.stages | other.stages, scope.access | other.access, scope.queueFamilyIndex};
    }

    /**
     * @return the access after the state needs a barrier
     */
    [[nodiscard]] constexpr bool needs_barrier(ImageState const& last, ImageState const& next, bool discard) {
        return discard || last.layout != next.layout || writes(last.scope.access) || writes(next.scope.access);
    }

    [[nodiscard]] constexpr bool needs_barrier(BufferState const& last, BufferState const& next) {
        return writes(last.scope.access) || writes(next.scope.access);
    }

    /**
     * @return the mip level count, `VK_REMAINING_MIP_LEVELS` resolved
     */
    [[nodiscard]] uint32_t mip_count(Image const& image, uint32_t firstMipLevel, uint32_t mipLevels) {
        Image::debug_check(image);
        CTH_CRITICAL(
            firstMipLevel >= image.mipLevels(),
            "first mip level must lie in the image ({}), got {}",
            image.mipLevels(),
            firstMipLevel
        ) {}

        uint32_t const count = mipLevels == VK_REMAINING_MIP_LEVELS ? image.mipLevels() - firstMipLevel : mipLevels;
        CTH_CRITICAL(
            count == 0 || count > image.mipLevels() - firstMipLevel,
            "mip levels must lie in the image ({}), got {} from {}",
            image.mipLevels(),
            count,
            firstMipLevel
        ) {}
        return count;
    }

    /**
     * @return the barriers differ in the mip levels only, `next` starts after the levels of `last`
     */
    [[nodiscard]] constexpr bool next_mip_levels(VkImageMemoryBarrier2 const& last, VkImageMemoryBarrier2 const& next) {
        auto const& lastRange = last.subresourceRange;
        auto const& nextRange = next.subresourceRange;
        return last.image == next.image && last.oldLayout == next.oldLayout && last.newLayout == next.newLayout
            && last.srcStageMask == next.srcStageMask && last.srcAccessMask == next.srcAccessMask
            && last.dstStageMask == next.dstStageMask && last.dstAccessMask == next.dstAccessMask
            && last.srcQueueFamilyIndex == next.srcQueueFamilyIndex
            && last.dstQueueFamilyIndex == next.dstQueueFamilyIndex && lastRange.aspectMask == nextRange.aspectMask
            && lastRange.baseArrayLayer == nextRange.baseArrayLayer && lastRange.layerCount == nextRange.layerCount
            && lastRange.levelCount != VK_REMAINING_MIP_LEVELS
            && lastRange.baseMipLevel + lastRange.levelCount == nextRange.baseMipLevel;
    }

    /**
     * appends the barrier of one mip level, merges it into an equal barrier of the level before
     * @param[out] barrier
     */
    void add_image_barrier(
        cvk::Barrier& barrier,
        VkImage vkImage,
        VkImageAspectFlags aspectMask,
        cvk::ImageBarrierInfo const& info
    ) {
        barrier.add(vkImage, aspectMask, info);

        auto& images = barrier.images;
        if(images.size() < 2 || !next_mip_levels(images[images.size() - 2], images.back())) return;
        images[images.size() - 2].subresourceRange.levelCount += images.back().subresourceRange.levelCount;
        images.pop_back();
    }

    [[nodiscard]] cvk::ImageBarrierInfo image_barrier(
        ImageState const& last,
        ImageState const& next,
        bool discard,
        uint32_t mipLevel
    ) {
        return cvk::ImageBarrierInfo{
            .src = src_scope(last.scope),
            .dst = next.scope,
            .newLayout = next.layout,
            .oldLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : last.layout,
            .firstMipLevel = mipLevel,
            .mipLevels = 1,
        };
    }
}

void ResourceStates::access(Image const& image, ImageAccess const& access, cvk::Barrier& barrier) {
    auto const count = mip_count(image, access.firstMipLevel, access.mipLevels);
    ImageState const next{
        .layout = access.layout,
        .scope = BarrierScope{.stages = access.stages, .access = access.access},
    };

    for(uint32_t level = access.firstMipLevel; level < access.firstMipLevel + count; ++level) {
        auto const [it, inserted] = _images.try_emplace(
            image_key_t{image.get(), level},
            image_states{
                .aspectMask = image.aspectMask(),
                .sharingMode = image.config().sharingMode(),
                .firstOpt = next,
                .last = next,
                .discard = access.discard,
            }
        );
        if(inserted) continue;

        auto& states = it->second;
        if(needs_barrier(states.last, next, access.discard)) {
            add_image_barrier(
                barrier,
                image.get(),
                states.aspectMask,
                image_barrier(states.last, next, access.discard, level)
            );
            states.last = next;
            states.barrier = true;
            continue;
        }

        // read after read in the same layout
        states.last.scope = merge(states.last.scope, next.scope);
        if(!states.barrier && states.firstOpt.has_value())
            states.firstOpt->scope = merge(states.firstOpt->scope, next.scope);
    }
}

void ResourceStates::access(BaseBuffer const& buffer, BufferAccess const& access, cvk::Barrier& barrier) {
    BaseBuffer::debug_check(buffer);
    BufferState const next{.scope = BarrierScope{.stages = access.stages, .access = access.access}};

    auto const [it, inserted] = _buffers.try_emplace(
        buffer.get(),
        buffer_states{.sharingMode = buffer.config().sharingMode(), .firstOpt = next, .last = next}
    );
    if(inserted) return;

    auto& states = it->second;
    if(needs_barrier(states.last, next)) {
        barrier.add(buffer, cvk::BufferBarrierInfo{.src = src_scope(states.last.scope), .dst = next.scope});
        states.last = next;
        states.barrier = true;
        return;
    }

    states.last.scope = merge(states.last.scope, next.scope);
    if(!states.barrier && states.firstOpt.has_value())
        states.firstOpt->scope = merge(states.firstOpt->scope, next.scope);
}

void ResourceStates::assume(Image const& image, ImageState const& state, uint32_t firstMipLevel, uint32_t mipLevels) {
    auto const count = mip_count(image, firstMipLevel, mipLevels);

    for(uint32_t level = firstMipLevel; level < firstMipLevel + count; ++level) {
        auto const [it, inserted] = _images.try_emplace(
            image_key_t{image.get(), level},
            image_states{
                .aspectMask = image.aspectMask(),
                .sharingMode = image.config().sharingMode(),
                .firstOpt = std::nullopt,
                .last = state,
                .barrier = true,
            }
        );
        if(inserted) continue;

        auto& states = it->second;
        states.last = state;
        states.barrier = true;
    }
}

void ResourceStates::assume(BaseBuffer const& buffer, BufferState const& state) {
    BaseBuffer::debug_check(buffer);

    auto const [it, inserted] = _buffers.try_emplace(
        buffer.get(),
        buffer_states{
            .sharingMode = buffer.config().sharingMode(),
            .firstOpt = std::nullopt,
            .last = state,
            .barrier = true,
        }
    );
    if(inserted) return;

    auto& states = it->second;
    states.last = state;
    states.barrier = true;
}

void ResourceStates::extend(Image const& image, BarrierScope const& scope, uint32_t firstMipLevel, uint32_t mipLevels) {
    auto const count = mip_count(image, firstMipLevel, mipLevels);

    for(uint32_t level = firstMipLevel; level < firstMipLevel + count; ++level) {
        auto const it = _images.find(image_key_t{image.get(), level});
        if(it == _images.end()) continue;

        auto& states = it->second;
        states.last.scope = merge(states.last.scope, scope);
        if(!states.barrier && states.firstOpt.has_value())
            states.firstOpt->scope = merge(states.firstOpt->scope, scope);
    }
}

void ResourceStates::append(ResourceStates const& next, cvk::Barrier& barrier) {
    for(auto const& [key, nextStates] : next._images) {
        auto const [it, inserted] = _images.try_emplace(key, nextStates);
        if(inserted) continue;

        auto& states = it->second;

        // an assume before the first access of next: no barrier
        if(!nextStates.firstOpt.has_value()) {
            states.last = nextStates.last;
            states.barrier = true;
            continue;
        }

        if(needs_barrier(states.last, *nextStates.firstOpt, nextStates.discard)) {
            add_image_barrier(
                barrier,
                key.first,
                nextStates.aspectMask,
                image_barrier(states.last, *nextStates.firstOpt, nextStates.discard, key.second)
            );
            states.last = nextStates.last;
            states.barrier = true;
            continue;
        }

        // read after read in the same layout
        if(!states.barrier && states.firstOpt.has_value())
            states.firstOpt->scope = merge(states.firstOpt->scope, nextStates.firstOpt->scope);
        states.last = nextStates.barrier ? nextStates.last
            : ImageState{states.last.layout, merge(states.last.scope, nextStates.last.scope)};
        states.barrier |= nextStates.barrier;
    }

    for(auto const& [key, nextStates] : next._buffers) {
        auto const [it, inserted] = _buffers.try_emplace(key, nextStates);
        if(inserted) continue;

        auto& states = it->second;

        if(!nextStates.firstOpt.has_value()) {
            states.last = nextStates.last;
            states.barrier = true;
            continue;
        }

        if(needs_barrier(states.last, *nextStates.firstOpt)) {
            barrier.add(
                key,
                cvk::BufferBarrierInfo{.src = src_scope(states.last.scope), .dst = nextStates.firstOpt->scope}
            );
            states.last = nextStates.last;
            states.barrier = true;
            continue;
        }

        if(!states.barrier && states.firstOpt.has_value())
            states.firstOpt->scope = merge(states.firstOpt->scope, nextStates.firstOpt->scope);
        states.last = nextStates.barrier ? nextStates.last
            : BufferState{merge(states.last.scope, nextStates.last.scope)};
        states.barrier |= nextStates.barrier;
    }
}

std::optional<ImageState> ResourceStates::first(VkImage vkImage, uint32_t mipLevel) const {
    auto const it = _images.find(image_key_t{vkImage, mipLevel});
    return it == _images.end() ? std::nullopt : it->second.firstOpt;
}

bool ResourceStates::discards(VkImage vkImage, uint32_t mipLevel) const {
    auto const it = _images.find(image_key_t{vkImage, mipLevel});
    return it != _images.end() && it->second.discard;
}

std::optional<ImageState> ResourceStates::last(VkImage vkImage, uint32_t mipLevel) const {
    auto const it = _images.find(image_key_t{vkImage, mipLevel});
    return it == _images.end() ? std::nullopt : std::optional{it->second.last};
}

std::optional<BufferState> ResourceStates::first(VkBuffer vkBuffer) const {
    auto const it = _buffers.find(vkBuffer);
    return it == _buffers.end() ? std::nullopt : it->second.firstOpt;
}

std::optional<BufferState> ResourceStates::last(VkBuffer vkBuffer) const {
    auto const it = _buffers.find(vkBuffer);
    return it == _buffers.end() ? std::nullopt : std::optional{it->second.last};
}

std::optional<ImageState> ResourceStates::first(Image const& image, uint32_t mipLevel) const {
    return first(image.get(), mipLevel);
}

bool ResourceStates::discards(Image const& image, uint32_t mipLevel) const { return discards(image.get(), mipLevel); }

std::optional<ImageState> ResourceStates::last(Image const& image, uint32_t mipLevel) const {
    return last(image.get(), mipLevel);
}

std::optional<BufferState> ResourceStates::first(BaseBuffer const& buffer) const { return first(buffer.get()); }

std::optional<BufferState> ResourceStates::last(BaseBuffer const& buffer) const { return last(buffer.get()); }

void ResourceStates::erase(Image const& image) {
    for(uint32_t level = 0; level < image.mipLevels(); ++level)
        erase(image.get(), level);
}

void ResourceStates::erase(BaseBuffer const& buffer) { erase(buffer.get()); }

void ResourceStates::erase(VkImage vkImage, uint32_t mipLevel) { _images.erase(image_key_t{vkImage, mipLevel}); }

void ResourceStates::erase(VkBuffer vkBuffer) { _buffers.erase(vkBuffer); }

bool ResourceStates::contains(Image const& image) const {
    for(uint32_t level = 0; level < image.mipLevels(); ++level)
        if(_images.contains(image_key_t{image.get(), level})) return true;
    return false;
}

bool ResourceStates::contains(BaseBuffer const& buffer) const { return _buffers.contains(buffer.get()); }
}
