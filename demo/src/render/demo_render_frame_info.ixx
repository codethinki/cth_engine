export module demo.render.frame_info;

import cth.vk.render.rec;

export namespace cth {

struct FrameInfo {
    size_t frameIndex;
    float frameTime;
    vk::PrimaryCmdBuffer const* commandBuffer;
};

} // namespace cth