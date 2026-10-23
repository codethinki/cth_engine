#include "jolly/render/submit/cmd/secondary_cmd_buffer.hpp"

#include "jvk/render/cmd/cmd_buffer.hpp"

namespace jly {

SecondaryCmdBuffer::SecondaryCmdBuffer(Config config, jvk::CmdPool& pool) :
    _handle{std::make_unique<jvk::SecondaryCmdBuffer>(std::move(config.baseConfig), pool)},
    _taskReuse{config.taskReuse} {}

SecondaryCmdBuffer::SecondaryCmdBuffer(Config config) :
    _handle{std::make_unique<jvk::SecondaryCmdBuffer>(std::move(config.baseConfig))},
    _taskReuse{config.taskReuse} {}
SecondaryCmdBuffer::~SecondaryCmdBuffer() = default;

// ReSharper disable CppMemberFunctionMayBeConst
void SecondaryCmdBuffer::create(jvk::CmdPool& pool) { _handle->create(pool); }
void SecondaryCmdBuffer::begin(
    jvk::RenderPass const& render_pass,
    jvk::Subpass const& subpass,
    jvk::Framebuffer const* framebuffer
) { _handle->begin(render_pass, subpass, framebuffer); }

void SecondaryCmdBuffer::end() { _handle->end(); }

void SecondaryCmdBuffer::reset(bool release_memory) {
    VkCommandBufferResetFlags const flags = release_memory ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;

    _handle->reset(flags);

    task_mixin::discardTasks();
}
void SecondaryCmdBuffer::destroy() {
    _handle->destroy();

    task_mixin::discardTasks();
}
// ReSharper restore CppMemberFunctionMayBeConst

bool SecondaryCmdBuffer::created() const { return _handle->created(); }
bool SecondaryCmdBuffer::recording() const { return _handle->recording(); }
jvk::CmdPool& SecondaryCmdBuffer::pool() const { return _handle->pool(); }

}
