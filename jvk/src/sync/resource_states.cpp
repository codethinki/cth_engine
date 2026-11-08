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
     * appends the barrier of one mip level, merges it into an equal barrier of the level before
     */
    void add_image_barrier(cvk::Barrier& barrier, Image const& image, cvk::ImageBarrierInfo const& info) {
        if(!barrier.images.empty()) {
            auto& [lastImage, lastInfo] = barrier.images.back();
            bool const same = lastImage.get() == &image && lastInfo.oldLayout == info.oldLayout
                && lastInfo.newLayout == info.newLayout && lastInfo.src.stages == info.src.stages
                && lastInfo.src.access == info.src.access && lastInfo.dst.stages == info.dst.stages
                && lastInfo.dst.access == info.dst.access
                && lastInfo.src.queueFamilyIndex == info.src.queueFamilyIndex
                && lastInfo.dst.queueFamilyIndex == info.dst.queueFamilyIndex
                && lastInfo.mipLevels != VK_REMAINING_MIP_LEVELS
                && lastInfo.firstMipLevel + lastInfo.mipLevels == info.firstMipLevel;
            if(same) {
                lastInfo.mipLevels += info.mipLevels;
                return;
            }
        }
        barrier.add(image, info);
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
            image_states{.imagePtr = &image, .firstOpt = next, .last = next, .discard = access.discard}
        );
        if(inserted) continue;

        auto& states = it->second;
        states.imagePtr = &image;

        if(needs_barrier(states.last, next, access.discard)) {
            add_image_barrier(barrier, image, image_barrier(states.last, next, access.discard, level));
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
        buffer_states{.bufferPtr = &buffer, .firstOpt = next, .last = next}
    );
    if(inserted) return;

    auto& states = it->second;
    states.bufferPtr = &buffer;

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
            image_states{.imagePtr = &image, .firstOpt = std::nullopt, .last = state, .barrier = true}
        );
        if(inserted) continue;

        auto& states = it->second;
        states.imagePtr = &image;
        states.last = state;
        states.barrier = true;
    }
}

void ResourceStates::assume(BaseBuffer const& buffer, BufferState const& state) {
    BaseBuffer::debug_check(buffer);

    auto const [it, inserted] = _buffers.try_emplace(
        buffer.get(),
        buffer_states{.bufferPtr = &buffer, .firstOpt = std::nullopt, .last = state, .barrier = true}
    );
    if(inserted) return;

    auto& states = it->second;
    states.bufferPtr = &buffer;
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
        states.imagePtr = nextStates.imagePtr;

        // an assume before the first access of next: no barrier
        if(!nextStates.firstOpt.has_value()) {
            states.last = nextStates.last;
            states.barrier = true;
            continue;
        }

        if(needs_barrier(states.last, *nextStates.firstOpt, nextStates.discard)) {
            add_image_barrier(
                barrier,
                *nextStates.imagePtr,
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
        states.bufferPtr = nextStates.bufferPtr;

        if(!nextStates.firstOpt.has_value()) {
            states.last = nextStates.last;
            states.barrier = true;
            continue;
        }

        if(needs_barrier(states.last, *nextStates.firstOpt)) {
            barrier.add(
                *nextStates.bufferPtr,
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

std::optional<ImageState> ResourceStates::first(Image const& image, uint32_t mipLevel) const {
    auto const it = _images.find(image_key_t{image.get(), mipLevel});
    return it == _images.end() ? std::nullopt : it->second.firstOpt;
}

bool ResourceStates::discards(Image const& image, uint32_t mipLevel) const {
    auto const it = _images.find(image_key_t{image.get(), mipLevel});
    return it != _images.end() && it->second.discard;
}

std::optional<ImageState> ResourceStates::last(Image const& image, uint32_t mipLevel) const {
    auto const it = _images.find(image_key_t{image.get(), mipLevel});
    return it == _images.end() ? std::nullopt : std::optional{it->second.last};
}

std::optional<BufferState> ResourceStates::first(BaseBuffer const& buffer) const {
    auto const it = _buffers.find(buffer.get());
    return it == _buffers.end() ? std::nullopt : it->second.firstOpt;
}

std::optional<BufferState> ResourceStates::last(BaseBuffer const& buffer) const {
    auto const it = _buffers.find(buffer.get());
    return it == _buffers.end() ? std::nullopt : std::optional{it->second.last};
}

void ResourceStates::erase(Image const& image) {
    for(uint32_t level = 0; level < image.mipLevels(); ++level)
        _images.erase(image_key_t{image.get(), level});
}

void ResourceStates::erase(BaseBuffer const& buffer) { _buffers.erase(buffer.get()); }

bool ResourceStates::contains(Image const& image) const {
    for(uint32_t level = 0; level < image.mipLevels(); ++level)
        if(_images.contains(image_key_t{image.get(), level})) return true;
    return false;
}

bool ResourceStates::contains(BaseBuffer const& buffer) const { return _buffers.contains(buffer.get()); }
}
