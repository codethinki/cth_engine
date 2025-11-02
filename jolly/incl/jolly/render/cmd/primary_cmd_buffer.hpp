#pragma once
#include "task_container.hpp"

namespace jvk {
class CmdPool;
class PrimaryCmdBuffer;
class CmdBufferConfig;
}

namespace jly {

class PrimaryCmdBuffer {
public:
    using Config = jvk::CmdBufferConfig;

    using completion_task_t = task_container::task_t;

    PrimaryCmdBuffer(Config const& config, jvk::CmdPool& pool);
    explicit PrimaryCmdBuffer(Config const& config);
    ~PrimaryCmdBuffer();

    void create(jvk::CmdPool& pool);

    /**
     * adds a task to execute once the buffer finished gpu execution (thread safe)
     */
    void addCompletionTask(completion_task_t task) { _tasks.add(std::move(task)); }
    /**
     * executes all added tasks and clears the list (thread safe)
     */
    void execCompletionTasks() { _tasks.execute(); }

    /**
     * discards all added tasks (thread safe)
     */
    void discardCompletionTasks() { _tasks.discard(); }

    void begin();
    void end();

    void reset(bool release_memory = false);

    void optDestroy() { if(created()) destroy(); }
    void destroy();

private:
    std::unique_ptr<jvk::PrimaryCmdBuffer> _handle;

    task_container _tasks;

public:
    [[nodiscard]] jvk::PrimaryCmdBuffer const& raw() const { return *_handle; }
    [[nodiscard]] bool created() const;
    [[nodiscard]] bool recording() const;
    [[nodiscard]] jvk::CmdPool& pool() const;

    PrimaryCmdBuffer(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer const& other) = delete;
    PrimaryCmdBuffer(PrimaryCmdBuffer&& other) noexcept = default;
    PrimaryCmdBuffer& operator=(PrimaryCmdBuffer&& other) noexcept = default;

};
}
