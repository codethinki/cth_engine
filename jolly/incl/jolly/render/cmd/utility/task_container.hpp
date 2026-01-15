#pragma once
#include <functional>

namespace jly {

namespace dev {
    template<class VoidCallable>
    concept void_callable = requires(VoidCallable void_func) {
        { void_func() };
    };

    template<class CompatibleVoidCallable, class VoidCallable>
    concept compatible_void_callable =
        void_callable<VoidCallable>
        && void_callable<CompatibleVoidCallable>
        && std::constructible_from<VoidCallable, CompatibleVoidCallable>;
}


template<dev::void_callable F>
struct basic_task_container {
    using task_type = F;

    basic_task_container() = default;
    ~basic_task_container() = default;

    void append(task_type task) { _tasks.emplace_back(std::move(task)); }

    template<dev::compatible_void_callable<F> U>
    void append(basic_task_container<U> const& other) { append(std::as_const(other.tasks())); }
    template<dev::compatible_void_callable<F> U>
    void append(basic_task_container<U>&& other) { append(std::move(other.tasks())); }

    template<cth::mta::range_over_cpt<CPT(dev::compatible_void_callable<F>)> Rng>
    void append(Rng&& rng) { _tasks.append_range(std::forward<Rng>(rng)); }


    void exec_clear() {
        exec();
        clear();
    }

    void clear() { _tasks.clear(); }

    void exec() const {
        for(auto& task : _tasks)
            task();
    }

    void discard() { _tasks.clear(); }

private:
    std::vector<task_type> _tasks;

public:
    auto const& tasks() const { return _tasks; }
    auto& tasks() { return _tasks; }


    basic_task_container(basic_task_container const& other) requires std::copy_constructible<F> = default;
    basic_task_container(basic_task_container const& other) requires !std::copy_constructible<F> = delete;
    basic_task_container& operator=(basic_task_container const& other) requires std::copy_constructible<F> = default;
    basic_task_container& operator=(basic_task_container const& other) requires !std::copy_constructible<F> = delete;

    basic_task_container(basic_task_container&& other) noexcept = default;
    basic_task_container& operator=(basic_task_container&& other) noexcept = default;
};

using task_container = basic_task_container<std::move_only_function<void()>>;

using copyable_task_container = basic_task_container<std::function<void()>>;


}
