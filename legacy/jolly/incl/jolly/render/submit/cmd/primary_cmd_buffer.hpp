#pragma once
#include "jolly/render/submit/cmd/utility/task_container_mixin.hpp"

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

class PrimaryCmdBuffer : dev::task_container_mixin {
    using task_mixin = dev::task_container_mixin;

public:
    using Config = jvk::CmdBufferConfig;

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

    using task_mixin::add;
    using task_mixin::append;
    using task_mixin::releaseTasks;
    using task_mixin::execClearTasks;
    using task_mixin::discardTasks;

private:
    std::unique_ptr<jvk::PrimaryCmdBuffer> _handle;

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
