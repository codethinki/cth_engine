#pragma once
#include <mutex>

namespace jvk {
class CmdPool;
class PrimaryCmdBuffer;
class CmdBufferConfig;
}

namespace jly {

class PrimaryCmdBuffer {
public:
    using Config = jvk::CmdBufferConfig;

    using completion_task_t = std::function<void()>;

    PrimaryCmdBuffer(Config const& config, jvk::CmdPool& pool);
    PrimaryCmdBuffer(Config const& config);

    void create(jvk::CmdPool& pool);

    void addTask(completion_task_t task) { _tasks.emplace_back(std::move(task)); }

    void begin();
    void end();

    void reset(bool release_memory = false);

private:
    std::unique_ptr<jvk::PrimaryCmdBuffer> _handle;

    std::vector<completion_task_t> _tasks;
    std::mutex _tasksMutex;

public:
    [[nodiscard]] jvk::PrimaryCmdBuffer const& raw() const { return *_handle; }
    [[nodiscard]] bool created() const;
    [[nodiscard]] bool recording() const;
    [[nodiscard]] jvk::CmdPool& pool() const;
    [[nodiscard]] VkBufferUsageFlags usageFlags() const;
};
}
