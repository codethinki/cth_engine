#pragma once

#include <vulkan/vulkan.h>

#include <unordered_map>
#include <vector>


//TEMP left off here use the barrier classes for the barriers
namespace cth {
using namespace std;

class BasicImage;
class DefaultBuffer;

class PipelineBarrier;

struct PipelineStages {
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_NONE;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_NONE;
};

struct PipelineAccess {
    VkAccessFlags accessMask = VK_ACCESS_NONE;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};
} // namespace cth

//ImageBarrier

namespace cth {
class ImageBarrier : protected PipelineStages {
public:
    struct Info;
    explicit ImageBarrier(PipelineStages stages, const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    explicit ImageBarrier(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
        const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    virtual ~ImageBarrier() = default;

    void add(BasicImage* image, const Info& info);
    void remove(BasicImage* image);

    virtual void execute(VkCommandBuffer cmd_buffer);

protected:
    void applyChanges() const;
    explicit ImageBarrier(const unordered_map<BasicImage*, ImageBarrier::Info>& images);

private:
    void init(const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    vector<BasicImage*> images{};
    vector<VkImageMemoryBarrier> imageBarriers{};

    vector<size_t> layoutChanges{};

    friend PipelineBarrier;
};

} // namespace cth

//BufferBarrier

namespace cth {

class BufferBarrier : protected PipelineStages {
public:
    struct Info;
    BufferBarrier(PipelineStages stages, const unordered_map<const DefaultBuffer*, Info>& buffers);
    BufferBarrier(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, const unordered_map<const DefaultBuffer*, Info>& buffers);

    virtual ~BufferBarrier() = default;

    void add(const DefaultBuffer* buffer, const Info& info);
    void remove(const DefaultBuffer* buffer);

    virtual void execute(VkCommandBuffer cmd_buffer);

protected:
    explicit BufferBarrier(const unordered_map<const DefaultBuffer*, Info>& buffers);

private:
    void init(const unordered_map<const DefaultBuffer*, Info>& buffers);
    vector<const DefaultBuffer*> buffers{};
    vector<VkBufferMemoryBarrier> bufferBarriers{};

    friend PipelineBarrier;
};
} // namespace cth

//Barrier

namespace cth {

class PipelineBarrier : public BufferBarrier, ImageBarrier {
public:
    PipelineBarrier(PipelineStages stages, const unordered_map<const DefaultBuffer*, BufferBarrier::Info>& buffers,
        const unordered_map<BasicImage*, ImageBarrier::Info>& images);
    PipelineBarrier(VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
        const unordered_map<const DefaultBuffer*, BufferBarrier::Info>& buffers,
        const unordered_map<BasicImage*, ImageBarrier::Info>& images);


    void execute(VkCommandBuffer cmd_buffer) override;
};
} // namespace cth

//Info

namespace cth {
struct ImageBarrier::Info {
    VkImageAspectFlagBits aspectMask = VK_IMAGE_ASPECT_NONE;
    uint32_t firstMipLevel = 0;
    uint32_t levels = 0; //0 => all remaining
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED; //VK_IMAGE_LAYOUT_UNDEFINED => old layout

    PipelineAccess src{};
    PipelineAccess dst{};

    static Info Default() { return Info{}; }

    static Info QueueTransition(const PipelineAccess& src, const PipelineAccess& dst) {
        return Info{VK_IMAGE_ASPECT_NONE, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, src, dst};
    }
    static Info QueueTransition(const VkAccessFlags src_access, const uint32_t src_queue_index, const VkAccessFlags dst_access, const uint32_t dst_queue_index) {
        return Info{VK_IMAGE_ASPECT_NONE, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, {src_access, src_queue_index}, {dst_access, dst_queue_index}};
    }
    static Info LayoutTransition(const VkAccessFlags src_access, const VkAccessFlags dst_access, const VkImageLayout new_layout,
        const uint32_t first_mip_level = 0, const uint32_t levels = 0) {
        Info info{};
        info.src.accessMask = src_access;
        info.dst.accessMask = dst_access;
        info.newLayout = new_layout;
        info.firstMipLevel = first_mip_level;
        info.levels = levels;
        return info;
    }
};

struct BufferBarrier::Info {
    PipelineAccess src{};
    PipelineAccess dst{};


    static Info Default() { return Info{}; }
    static Info QueueTransition(const PipelineAccess& src, const PipelineAccess& dst) { return Info{src, dst}; }
};
}
