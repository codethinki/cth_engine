#pragma once
#include <functional>
#include <mutex>

namespace jly {
struct thread_safe_task_container {
    using task_t = std::function<void()>;

    thread_safe_task_container() = default;
    ~thread_safe_task_container() = default;

    void append(task_t task) {
        std::scoped_lock _{_tasksMtx};
        _tasks.emplace_back(std::move(task));
    }


    void append(thread_safe_task_container const& other) {
        std::scoped_lock _{_tasksMtx, other._tasksMtx};
        append(std::as_const(other._tasks));
    }
    void append(thread_safe_task_container&& other) {
        std::scoped_lock _{_tasksMtx};
        append(std::move(other._tasks));
    }

    template<cth::mta::range_over<task_t> Rng>
    void append(Rng&& rng) {
        std::scoped_lock _{_tasksMtx};
        _tasks.append_range(std::forward<Rng>(rng));
    }


    void exec_clear() {
        exec();
        clear();
    }

    void clear() {
        std::scoped_lock _{_tasksMtx};
        _tasks.clear();
    }

    void exec() const {
        std::scoped_lock _{_tasksMtx};
        for(auto& task : _tasks)
            task();
    }

    void discard() {
        std::scoped_lock _{_tasksMtx};
        _tasks.clear();
    }

private:
    std::vector<task_t> _tasks;
    mutable std::mutex _tasksMtx;

public:
    thread_safe_task_container(thread_safe_task_container const& other) {
        std::scoped_lock _{other._tasksMtx};
        _tasks = other._tasks; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }
    thread_safe_task_container& operator=(thread_safe_task_container const& other) {
        auto& self = *this;
        if(this == &other) return self;

        std::scoped_lock _{self._tasksMtx, other._tasksMtx};
        self._tasks = other._tasks;
        return self;
    }
    thread_safe_task_container(thread_safe_task_container&& other) noexcept {
        std::scoped_lock _{other._tasksMtx};
        _tasks = std::exchange(other._tasks, {});
    }
    thread_safe_task_container& operator=(thread_safe_task_container&& other) noexcept {
        auto& self = *this;

        if(this == &other) return self;

        std::scoped_lock _{self._tasksMtx, other._tasksMtx};
        self._tasks = std::exchange(other._tasks, {});
        return self;
    }
};

}
