module;
#include "lib/volk.hpp"
#include <cstdint>
export module cth.vk.render.sync.pipeline_barrier;


import cth.vk.constants;
import cth.vk.base.queue;
import cth.vk.base.core;
import cth.vk.res.img.image;
import cth.vk.res.buffer.base;
import cth.vk.render.rec.cmd.buffer;
import cth.vk.render.sync.barrier.base;

import cth.ptr.not_null;

import std;





//BufferBarrier

export namespace cth::vk {

class BufferBarrier : virtual protected BarrierBase {
public:
    struct Info;
    BufferBarrier(Core const& core, PipelineStages stages) : BarrierBase{core, stages} {}
    BufferBarrier(Core const& core, PipelineStages stages, std::unordered_map<BaseBuffer const*, Info> const& buffers);

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

export namespace cth::vk {

class PipelineBarrier : public BufferBarrier, public ImageBarrier {
public:
    explicit PipelineBarrier(Core const& core, PipelineStages stages) : BarrierBase{core, stages},
        BufferBarrier{core, stages}, ImageBarrier{core, stages} {}


    PipelineBarrier(Core const& core, PipelineStages stages, std::unordered_map<BaseBuffer const*, BufferBarrier::Info> const& buffers,
        std::unordered_map<Image*, ImageBarrier::Info> const& images);

    void execute(CmdBuffer const& cmd_buffer) override;

private:
    void initStages(PipelineStages stages);
};
} // namespace cth

//Info

export namespace cth::vk {

struct BufferBarrier::Info {
    PipelineAccess src{};
    PipelineAccess dst{};


    static Info Default() { return Info{}; }
    static Info QueueTransition(PipelineAccess const& src, PipelineAccess const& dst) { return Info{src, dst}; }
};
}
