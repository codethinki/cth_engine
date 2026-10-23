#pragma once

#include "jolly/render/submit/cmd/utility/task_container.hpp"

namespace jly::dev {
template<class F>
class basic_task_container_mixin {
public:
    using task_type = F;
    using container_type = std::vector<task_type>;

    /**
     * adds a task to execute once the buffer finished gpu execution
     */
    void add(task_type task) { _tasks.append(std::move(task)); }

    /**
     * adds a range of tasks to execute once the buffer finished gpu execution
     */
    template<cth::mta::range_over_cpt<CPT(cth::mta::constructs<task_type>)> Rng>
    void append(Rng&& rng) { _tasks.append_range(std::forward<Rng>(rng)); }

    template<class C>
    void append(C&& container) { _tasks.append(container); }

    /**
     * releases all tasks
     */
    container_type releaseTasks() { return std::exchange(_tasks, {}); }

    /**
     * executes and clears all tasks
     */
    void execClearTasks() {
        execTasks();
        discardTasks();
    }

    /**
     * executes all tasks
     */
    void execTasks() {
        for(auto& task : _tasks)
            task();
    }

    /**
     * discards all added tasks
     */
    void discardTasks() { _tasks.clear(); }


    container_type copyTasks() requires std::copy_constructible<container_type> {
        return _tasks;
    }

private:
    std::vector<task_type> _tasks{};
};

using task_signature = void();

using task_container_mixin = basic_task_container_mixin<std::move_only_function<task_signature>>;
using copyable_task_container_mixin = basic_task_container_mixin<std::function<task_signature>>;

}
