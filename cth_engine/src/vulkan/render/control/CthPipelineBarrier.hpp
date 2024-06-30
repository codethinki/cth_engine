#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>

#include "vulkan/base/CthQueue.hpp"
#include "vulkan/utility/CthConstants.hpp"

namespace cth {
class Queue;
}

namespace cth {
using namespace std;

class BasicImage;
class BasicBuffer;

class CmdBuffer;

class PipelineBarrier;

struct PipelineStages {

    VkPipelineStageFlags srcStage = Constant::PIPELINE_STAGE_IGNORED;
    VkPipelineStageFlags dstStage = Constant::PIPELINE_STAGE_IGNORED;
};

struct PipelineAccess {
    VkAccessFlags accessMask = Constant::DEFAULT_ACCESS;
    uint32_t queueFamilyIndex = Constant::QUEUE_FAMILY_IGNORED;
};
} // namespace cth

//ImageBarrier

namespace cth {
class ImageBarrier : virtual protected PipelineStages {
public:
    struct Info;
    explicit ImageBarrier(const PipelineStages stages) : PipelineStages(stages) {}
    explicit ImageBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) : PipelineStages{src_stage, dst_stage} {}
    explicit ImageBarrier(PipelineStages stages, const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    explicit ImageBarrier(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
        const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    virtual ~ImageBarrier() = default;

    void add(BasicImage* image, const Info& info);
    void replace(BasicImage* image, const Info& info);
    void remove(const BasicImage* image);

    virtual void execute(const CmdBuffer& cmd_buffer);

    [[nodiscard]] bool contains(const BasicImage* image) const;

protected:
    void applyChanges() const;
    ImageBarrier() = default;
    explicit ImageBarrier(const unordered_map<BasicImage*, Info>& images);

private:
    [[nodiscard]] ptrdiff_t find(const BasicImage* image) const;

    void removeChange(size_t barrier_index);
    void init(const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    vector<VkImageMemoryBarrier> _imageBarriers{};

    vector<std::pair<size_t, BasicImage*>> _layoutChanges{};

    friend PipelineBarrier;

public:
    ImageBarrier(const ImageBarrier& other) = delete;
    ImageBarrier& operator=(const ImageBarrier& other) = delete;
    ImageBarrier(ImageBarrier&& other) noexcept = default;
    ImageBarrier& operator=(ImageBarrier&& other) noexcept = default;
};

} // namespace cth

//BufferBarrier

namespace cth {

class BufferBarrier : virtual protected PipelineStages {
public:
    struct Info;
    explicit BufferBarrier(const PipelineStages stages) : PipelineStages(stages) {}
    explicit BufferBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) : PipelineStages{src_stage, dst_stage} {}
    BufferBarrier(PipelineStages stages, const unordered_map<const BasicBuffer*, Info>& buffers);
    BufferBarrier(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, const unordered_map<const BasicBuffer*, Info>& buffers);

    virtual ~BufferBarrier() = default;

    void add(const BasicBuffer* buffer, const Info& info);
    void remove(const BasicBuffer* buffer);

    virtual void execute(const CmdBuffer& cmd_buffer);

protected:
    explicit BufferBarrier(const unordered_map<const BasicBuffer*, Info>& buffers);
    BufferBarrier() = default;

private:
    void init(const unordered_map<const BasicBuffer*, Info>& buffers);
    vector<const BasicBuffer*> _buffers{};
    vector<VkBufferMemoryBarrier> _bufferBarriers{};

    friend PipelineBarrier;

public:
    BufferBarrier(const BufferBarrier& other) = delete;
    BufferBarrier& operator=(const BufferBarrier& other) = delete;
    BufferBarrier(BufferBarrier&& other) noexcept = default;
    BufferBarrier& operator=(BufferBarrier&& other) noexcept = default;
};
} // namespace cth

//Barrier

namespace cth {

class PipelineBarrier : public BufferBarrier, public ImageBarrier {
public:
    explicit PipelineBarrier(const PipelineStages stages) : BufferBarrier(stages), ImageBarrier(stages) {
        srcStage = stages.srcStage;
        dstStage = stages.dstStage;
    }
    explicit PipelineBarrier(const VkPipelineStageFlags src_stage, const VkPipelineStageFlags dst_stage) {
        srcStage = src_stage;
        dstStage = dst_stage;
    }


    PipelineBarrier(PipelineStages stages, const unordered_map<const BasicBuffer*, BufferBarrier::Info>& buffers,
        const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    PipelineBarrier(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
        const unordered_map<const BasicBuffer*, BufferBarrier::Info>& buffers,
        const unordered_map<BasicImage*, ImageBarrier::Info>& images);


    void execute(const CmdBuffer& cmd_buffer) override;

private:
    void initStages(const PipelineStages stages) { initStages(stages.srcStage, stages.dstStage); }
    void initStages(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage);
};
} // namespace cth

//Info

namespace cth {
struct ImageBarrier::Info {


    VkImageAspectFlagBits aspectMask = Constant::ASPECT_MASK_IGNORED; //Constants::ASPECT_MASK_IGNORED => image default aspect
    uint32_t firstMipLevel = 0;
    uint32_t levels = Constant::ALL; //Constants::ALL => all remaining
    VkImageLayout newLayout = Constant::IMAGE_LAYOUT_IGNORED; //Constants::IMAGE_LAYOUT_IGNORED => old layout

    PipelineAccess src;
    PipelineAccess dst;


    static Info Default() { return Info{}; }

    static Info QueueTransition(const PipelineAccess& src, const PipelineAccess& dst) { return Info{.src = src, .dst = dst}; }
    static Info QueueTransition(const VkAccessFlags src_access, const uint32_t src_queue_family_index, const VkAccessFlags dst_access,
        const uint32_t dst_queue_family_index) {
        return Info{
            .src = {src_access, src_queue_family_index},
            .dst = {dst_access, dst_queue_family_index}
        };
    }
    static Info QueueTransition(const VkAccessFlags src_access, const Queue& src_queue, const VkAccessFlags dst_access,
        const Queue& dst_queue) {
        return Info{
            .src = {src_access, src_queue.familyIndex()},
            .dst = {dst_access, dst_queue.familyIndex()}
        };
    }
    /**
     * @param levels (Constants::ALL => all remaining)
     */
    static Info LayoutTransition(const VkImageLayout new_layout, const VkAccessFlags src_access, const VkAccessFlags dst_access,
        const uint32_t first_mip_level = 0, const uint32_t levels = 0) {
        return Info{
            .firstMipLevel = first_mip_level,
            .levels = levels,
            .newLayout = new_layout,
            .src = PipelineAccess{src_access},
            .dst = PipelineAccess{dst_access},
        };
    }

private:
    [[nodiscard]] VkImageMemoryBarrier createBarrier(const BasicImage& image) const;

    friend ImageBarrier;
};

struct BufferBarrier::Info {
    PipelineAccess src{};
    PipelineAccess dst{};


    static Info Default() { return Info{}; }
    static Info QueueTransition(const PipelineAccess& src, const PipelineAccess& dst) { return Info{src, dst}; }
};
}
