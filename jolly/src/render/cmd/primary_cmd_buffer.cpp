#include "jolly/render/cmd/primary_cmd_buffer.hpp"

#include "jvk/render/cmd/cmd_buffer.hpp"

namespace jly {

PrimaryCmdBuffer::PrimaryCmdBuffer(Config const& config, jvk::CmdPool& pool) :
    _handle{std::make_unique<jvk::PrimaryCmdBuffer>(config, pool)} {}

PrimaryCmdBuffer::PrimaryCmdBuffer(Config const& config) :
    _handle{std::make_unique<jvk::PrimaryCmdBuffer>(config)} {}
PrimaryCmdBuffer::~PrimaryCmdBuffer() = default;

// ReSharper disable CppMemberFunctionMayBeConst
void PrimaryCmdBuffer::create(jvk::CmdPool& pool) { _handle->create(pool); }
void PrimaryCmdBuffer::begin() { _handle->begin(); }
void PrimaryCmdBuffer::end() { _handle->end(); }

void PrimaryCmdBuffer::reset(bool release_memory) {
    VkCommandBufferResetFlags const flags = release_memory ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;

    _handle->reset(flags);

    _tasks.discard();
}
void PrimaryCmdBuffer::destroy() {
    _handle->destroy();

    _tasks.discard();
}
// ReSharper restore CppMemberFunctionMayBeConst

bool PrimaryCmdBuffer::created() const { return _handle->created(); }
bool PrimaryCmdBuffer::recording() const { return _handle->recording(); }
jvk::CmdPool& PrimaryCmdBuffer::pool() const { return _handle->pool(); }



}
