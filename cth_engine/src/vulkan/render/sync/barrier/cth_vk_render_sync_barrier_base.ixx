export module cth.vk.render.sync.barrier.base;

import cth.vk.constants;
import cth.vk.base.core;

import cth.ptr.not_null;

import std;

export namespace cth::vk {
struct PipelineStages {

    VkPipelineStageFlags src = constants::PIPELINE_STAGE_IGNORED;
    VkPipelineStageFlags dst = constants::PIPELINE_STAGE_IGNORED;
};

struct PipelineAccess {
    VkAccessFlags accessMask = constants::DEFAULT_ACCESS;
    uint32_t queueFamilyIndex = constants::QUEUE_FAMILY_IGNORED;
};
} // namespace cth


export namespace cth::vk {
class BarrierBase {
public:
    BarrierBase(Core const& core, PipelineStages stages) : _core{&core}, _stages{stages} {}
    virtual ~BarrierBase() = default;

private:
    cth::not_null<Core const*> _core;
    PipelineStages _stages;

public:
    [[nodiscard]] VkPipelineStageFlags srcStage() const { return _stages.src; }
    [[nodiscard]] VkPipelineStageFlags dstStage() const { return _stages.dst; }
    [[nodiscard]] Core const& core() const { return *_core; }
    [[nodiscard]] PipelineStages stages() const { return _stages; }

};
}
