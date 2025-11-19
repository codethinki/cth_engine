#pragma once
#include "task_container.hpp"

namespace jly {
class SecondaryCmdBuffer;
}

namespace jvk {
struct DeviceTable;
class CmdPool;
class PrimaryCmdBuffer;
struct CmdBufferConfig;
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

    void begin();
    void end();


    void exec(std::span<SecondaryCmdBuffer> secondaries);

    /**
     * resets the cmd buffer and discards tasks
     * @param release_memory (releases memory to pool)
     */
    void reset(bool release_memory = false);

    void optDestroy() { if(created()) destroy(); }
    void destroy();

    /**
     * adds a task to execute once the buffer finished gpu execution (thread safe)
     */
    void add(completion_task_t task) { _tasks.append(std::move(task)); }

    /**
    * executes all added tasks. clears task list
    */
    void executeTasks() { _tasks.exec_clear(); }

    /**
     * discards all added tasks (thread safe)
     */
    void discardTasks() { _tasks.discard(); }

    /**
     * releases all tasks (thread safe)
     */
    task_container releaseTasks() { return std::exchange(_tasks, {}); }

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
