export module demo.render.frame_info;

import cth.vk.render.rec.cmd.buffer.primary;

export namespace cth {

struct FrameInfo {
    size_t frameIndex;
    float frameTime;
    vk::PrimaryCmdBuffer const* commandBuffer;
};

} // namespace cth