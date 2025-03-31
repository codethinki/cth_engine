#include "RenderStage.hpp"
#include "src/vulkan/base/queue/CthSubmitInfo.hpp"
#include "src/vulkan/render/cmd/CthCmdBuffer.hpp"
#include "src/vulkan/render/control/CthPipelineWaitStage.hpp"

namespace cth::vk {

RenderStage::RenderStage(Core const& core, Config config): _core{&core}, _queue{config.queue} {}
RenderStage::RenderStage(Core const& core, Config config, create_t) : RenderStage{core, config} {}
}
