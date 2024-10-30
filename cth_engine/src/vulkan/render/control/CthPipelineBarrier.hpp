#pragma once

#include <volk.h>

#include <unordered_map>
#include <vector>

#include "vulkan/base/queue/CthQueue.hpp"
#include "vulkan/utility/cth_constants.hpp"


namespace cth::vk {
class Queue;

class Image;
class BaseBuffer;
class CmdBuffer;

class PipelineBarrier;

struct PipelineStages {

    VkPipelineStageFlags src = constants::PIPELINE_STAGE_IGNORED;
    VkPipelineStageFlags dst = constants::PIPELINE_STAGE_IGNORED;
};

struct PipelineAccess {
    VkAccessFlags accessMask = constants::DEFAULT_ACCESS;
    uint32_t queueFamilyIndex = constants::QUEUE_FAMILY_IGNORED;
};
} // namespace cth


namespace cth::vk {
class BarrierBase {
public:
    BarrierBase(cth::not_null<Core const*> core, PipelineStages stages) : _core{core}, _stages{stages} {}

private:
    cth::not_null<Core const*> _core;
    PipelineStages _stages;

public:
    [[nodiscard]] VkPipelineStageFlags srcStage() const { return _stages.src; }
    [[nodiscard]] VkPipelineStageFlags dstStage() const { return _stages.dst; }
    [[nodiscard]] cth::not_null<Core const*> core() const { return _core; }
    [[nodiscard]] PipelineStages stages() const { return _stages; }

};
}

//ImageBarrier

namespace cth::vk {
class ImageBarrier : virtual protected BarrierBase {
public:
    struct Info;
    ImageBarrier(cth::not_null<Core const*> core, PipelineStages stages) : BarrierBase(core, stages) {}
    ImageBarrier(cth::not_null<Core const*> core, PipelineStages stages, std::unordered_map<Image*, Info> const& images);
    virtual ~ImageBarrier() = default;

    void add(Image* image, Info const& info);
    void replace(Image* image, Info const& info);
    void remove(Image const* image);

    virtual void execute(CmdBuffer const& cmd_buffer);

    [[nodiscard]] bool contains(Image const* image) const;

protected:
    void applyChanges() const;

private:
    [[nodiscard]] ptrdiff_t find(Image const* image) const;

    void removeChange(size_t barrier_index);
    void init(std::unordered_map<Image*, ImageBarrier::Info> const& images);
    std::vector<VkImageMemoryBarrier> _imageBarriers{};

    std::vector<std::pair<size_t, Image*>> _layoutChanges{};

    friend PipelineBarrier;

public:
    ImageBarrier(ImageBarrier const& other) = delete;
    ImageBarrier& operator=(ImageBarrier const& other) = delete;
    ImageBarrier(ImageBarrier&& other) noexcept = default;
    ImageBarrier& operator=(ImageBarrier&& other) noexcept = default;
};

} // namespace cth

//BufferBarrier

namespace cth::vk {

class BufferBarrier : virtual protected BarrierBase {
public:
    struct Info;
    BufferBarrier(cth::not_null<Core const*> core, PipelineStages stages) : BarrierBase{core, stages} {}
    BufferBarrier(cth::not_null<Core const*> core, PipelineStages stages, std::unordered_map<BaseBuffer const*, Info> const& buffers);

    virtual ~BufferBarrier() = default;

    void add(BaseBuffer const* buffer, Info const& info);
    void remove(BaseBuffer const* buffer);

    virtual void execute(CmdBuffer const& cmd_buffer);

private:
    void init(std::unordered_map<BaseBuffer const*, Info> const& buffers);
    std::vector<BaseBuffer const*> _buffers{};
    std::vector<VkBufferMemoryBarrier> _bufferBarriers{};

    friend PipelineBarrier;

public:
    BufferBarrier(BufferBarrier const& other) = delete;
    BufferBarrier& operator=(BufferBarrier const& other) = delete;
    BufferBarrier(BufferBarrier&& other) noexcept = default;
    BufferBarrier& operator=(BufferBarrier&& other) noexcept = default;
};
} // namespace cth

//Barrier

namespace cth::vk {

class PipelineBarrier : public BufferBarrier, public ImageBarrier {
public:
    explicit PipelineBarrier(cth::not_null<Core const*> core, PipelineStages stages) : BarrierBase{core, stages},
        BufferBarrier{core, stages}, ImageBarrier{core, stages} {}


    PipelineBarrier(cth::not_null<Core const*> core, PipelineStages stages, std::unordered_map<BaseBuffer const*, BufferBarrier::Info> const& buffers,
        std::unordered_map<Image*, ImageBarrier::Info> const& images);

    void execute(CmdBuffer const& cmd_buffer) override;

private:
    void initStages(PipelineStages stages);
};
} // namespace cth

//Info

namespace cth::vk {
struct ImageBarrier::Info {


    VkImageAspectFlagBits aspectMask = constants::ASPECT_MASK_IGNORED; //Constants::ASPECT_MASK_IGNORED => vk_image default aspect
    uint32_t firstMipLevel = 0;
    uint32_t levels = constants::ALL; //Constants::ALL => all remaining
    VkImageLayout newLayout = constants::IMAGE_LAYOUT_IGNORED; //Constants::IMAGE_LAYOUT_IGNORED => old layout

    PipelineAccess src;
    PipelineAccess dst;


    static Info Default() { return Info{}; }

    static Info QueueTransition(PipelineAccess const& src, PipelineAccess const& dst) { return Info{.src = src, .dst = dst}; }
    static Info QueueTransition(VkAccessFlags src_access, uint32_t src_queue_family_index, VkAccessFlags dst_access,
        uint32_t dst_queue_family_index) {
        return Info{
            .src = {src_access, src_queue_family_index},
            .dst = {dst_access, dst_queue_family_index}
        };
    }
    static Info QueueTransition(VkAccessFlags src_access, Queue const& src_queue, VkAccessFlags dst_access,
        Queue const& dst_queue) {
        return Info{
            .src = {src_access, src_queue.familyIndex()},
            .dst = {dst_access, dst_queue.familyIndex()}
        };
    }
    /**
     * @param levels (Constants::ALL => all remaining)
     */
    static Info LayoutTransition(VkImageLayout new_layout, VkAccessFlags src_access, VkAccessFlags dst_access,
        uint32_t const first_mip_level = 0, uint32_t const levels = 0) {
        return Info{
            .firstMipLevel = first_mip_level,
            .levels = levels,
            .newLayout = new_layout,
            .src = PipelineAccess{src_access},
            .dst = PipelineAccess{dst_access},
        };
    }

private:
    [[nodiscard]] VkImageMemoryBarrier createBarrier(Image const& image) const;

    friend ImageBarrier;
};

struct BufferBarrier::Info {
    PipelineAccess src{};
    PipelineAccess dst{};


    static Info Default() { return Info{}; }
    static Info QueueTransition(PipelineAccess const& src, PipelineAccess const& dst) { return Info{src, dst}; }
};
}
