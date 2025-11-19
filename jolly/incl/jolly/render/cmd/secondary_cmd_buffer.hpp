#pragma once
#include "secondary_cmd_buffer_config.hpp"
#include "task_container.hpp"

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

class SecondaryCmdBuffer {
public:
    using Config = SecondaryCmdBufferConfig;

    using completion_task_t = task_container::task_t;

    SecondaryCmdBuffer(Config config, jvk::CmdPool& pool);
    explicit SecondaryCmdBuffer(Config config);
    ~SecondaryCmdBuffer();

    void create(jvk::CmdPool& pool);

    void begin(jvk::RenderPass const&, jvk::Subpass const&, jvk::Framebuffer const*);
    void end();

    void exec(PrimaryCmdBuffer const& cmd_buffer);


    void reset(bool release_memory = false);

    void optDestroy() { if(created()) destroy(); }
    void destroy();

    /**
     * adds a task to execute once the buffer finished gpu execution (thread safe)
     */
    void add(completion_task_t task) { _tasks.append(std::move(task)); }

    /**
     * executes all added tasks. clears if not @ref Config::taskReuse
     */
    void executeTasks() { _taskReuse ? _tasks.exec() : _tasks.exec_clear(); }

    /**
     * discards all added tasks (thread safe)
     */
    void discardTasks() { _tasks.discard(); }

    /**
     * copies or moves (@ref SecondaryCmdBufferConfig::taskReuse) all tasks (thread safe)
     */
    task_container submitTasks() {
        if(_taskReuse) return _tasks;
        return std::exchange(_tasks, {});
    }

private:
    std::unique_ptr<jvk::SecondaryCmdBuffer> _handle;
    bool _taskReuse;


    task_container _tasks{};

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
