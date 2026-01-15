#pragma once
#include "secondary_cmd_buffer_config.hpp"
#include "jolly/render/cmd/utility/task_container_mixin.hpp"

namespace jly {
class PrimaryCmdBuffer;
}

namespace jvk {
class CmdPool;
class SecondaryCmdBuffer;
class RenderPass;
class Subpass;
class Framebuffer;
}

namespace jly {


class SecondaryCmdBuffer : dev::copyable_task_container_mixin {
    using task_mixin = dev::copyable_task_container_mixin;

public:
    using Config = SecondaryCmdBufferConfig;


    SecondaryCmdBuffer(Config config, jvk::CmdPool& pool);
    explicit SecondaryCmdBuffer(Config config);
    ~SecondaryCmdBuffer();

    void create(jvk::CmdPool& pool);

    void begin(jvk::RenderPass const&, jvk::Subpass const&, jvk::Framebuffer const*);
    void end();


    void reset(bool release_memory = false);

    void optDestroy() { if(created()) destroy(); }
    void destroy();

    using task_mixin::add;
    using task_mixin::append;
    using task_mixin::discardTasks;

    /**
     * executes all added tasks. clears if not @ref Config::taskReuse
     */
    void executeTasks() { _taskReuse ? task_mixin::execTasks() : task_mixin::execClearTasks(); }

    /**
     * copies or moves (@ref Config::taskReuse) all tasks
     */
    auto submitTasks() {
        if(_taskReuse) return copyTasks();
        return releaseTasks();
    }

private:
    std::unique_ptr<jvk::SecondaryCmdBuffer> _handle;
    bool _taskReuse;

public:
    [[nodiscard]] jvk::SecondaryCmdBuffer const& raw() const { return *_handle; }
    [[nodiscard]] bool created() const;
    [[nodiscard]] bool recording() const;
    [[nodiscard]] jvk::CmdPool& pool() const;

    SecondaryCmdBuffer(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer const& other) = delete;
    SecondaryCmdBuffer(SecondaryCmdBuffer&& other) noexcept = default;
    SecondaryCmdBuffer& operator=(SecondaryCmdBuffer&& other) noexcept = default;

};
}
