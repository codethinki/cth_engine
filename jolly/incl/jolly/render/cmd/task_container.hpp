#pragma once
#include <functional>
#include <mutex>

namespace jly {
struct task_container {
    using task_t = std::function<void()>;

    task_container() = default;
    ~task_container() = default;

    void append(task_t task) {
        std::lock_guard _{_tasksMtx};
        _tasks.emplace_back(std::move(task));
    }


    void append(task_container const& other) {
        std::scoped_lock _{_tasksMtx, other._tasksMtx};
        append(std::as_const(other._tasks));
    }
    void append(task_container&& other) {
        std::lock_guard _{_tasksMtx};
        append(std::move(other._tasks));
    }

    template<cth::type::range_over<task_t> Rng>
    void append(Rng&& rng) {
        std::lock_guard _{_tasksMtx};
        _tasks.append_range(std::forward<Rng>(rng));
    }


    void exec_clear() {
        exec();
        clear();
    }

    void clear() {
        std::lock_guard _{_tasksMtx};
        _tasks.clear();
    }

    void exec() const {
        std::lock_guard _{_tasksMtx};
        for(auto& task : _tasks)
            task();
    }

    void discard() {
        std::lock_guard _{_tasksMtx};
        _tasks.clear();
    }

private:
    std::vector<task_t> _tasks;
    mutable std::mutex _tasksMtx;

public:
    task_container(task_container const& other) {
        std::lock_guard _{other._tasksMtx};
        _tasks = other._tasks; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }
    task_container& operator=(task_container const& other) {
        auto& self = *this;
        if(this == &other) return self;

        std::scoped_lock _{self._tasksMtx, other._tasksMtx};
        self._tasks = other._tasks;
        return self;
    }
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
