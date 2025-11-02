#pragma once
#include <functional>
#include <mutex>

namespace jly {
struct task_container {
    using task_t = std::function<void()>;

    task_container() = default;
    ~task_container() = default;

    void add(task_t task) {
        std::lock_guard _{_tasksMtx};
        _tasks.emplace_back(std::move(task));
    }

    void execute() {
        std::lock_guard _{_tasksMtx};

        for(auto& task : _tasks)
            task();

        _tasks.clear();
    }

    void discard() {
        std::lock_guard _{_tasksMtx};
        _tasks.clear();
    }

private:
    std::vector<task_t> _tasks;
    std::mutex _tasksMtx;

public:
    task_container(task_container const& other) = delete;
    task_container& operator=(task_container const& other) = delete;
    task_container(task_container&& other) noexcept {
        std::lock_guard _{other._tasksMtx};
        _tasks = std::exchange(other._tasks, {});
    }
    task_container& operator=(task_container&& other) noexcept {
        auto& self = *this;

        if(this == &other) return self;

        std::scoped_lock _{self._tasksMtx, other._tasksMtx};
        self._tasks = std::exchange(other._tasks, {});
        return self;
    }
};
}
