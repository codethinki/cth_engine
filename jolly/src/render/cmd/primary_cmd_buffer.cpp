#include "jolly/render/cmd/primary_cmd_buffer.hpp"

#include "jolly/render/cmd/secondary_cmd_buffer.hpp"

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
void PrimaryCmdBuffer::exec(std::span<SecondaryCmdBuffer> secondaries) {
    for(auto& secondary : secondaries)
        task_mixin::append(secondary.submitTasks());

    std::vector const handles{
        std::from_range,
        secondaries | std::views::transform([](auto const& secondary) { return secondary.raw().get(); })
    };

    _handle->deviceTable()->vkCmdExecuteCommands(
        _handle->get(),
        static_cast<uint32_t>(handles.size()),
        handles.data()
    );
}

void PrimaryCmdBuffer::reset(bool release_memory) {
    VkCommandBufferResetFlags const flags = release_memory ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0;

    _handle->reset(flags);

    task_mixin::discardTasks();
}
void PrimaryCmdBuffer::destroy() {
    _handle->destroy();

    task_mixin::discardTasks();
}
// ReSharper restore CppMemberFunctionMayBeConst

bool PrimaryCmdBuffer::created() const { return _handle->created(); }
bool PrimaryCmdBuffer::recording() const { return _handle->recording(); }
jvk::CmdPool& PrimaryCmdBuffer::pool() const { return _handle->pool(); }



}
