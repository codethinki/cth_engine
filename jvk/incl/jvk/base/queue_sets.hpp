#pragma once
#include "cvk/base/core.hpp"
#include "cvk/base/queue/queue_family.hpp"

#include <cth/data/union_find.hpp>
#include <cth/io/log.hpp>

#include <cstddef>
#include <functional>
#include <span>
#include <utility>
#include <vector>


namespace jvk {
/**
 * creates the queue sets of `cvk::CoreConfig::queueSets` for the queues
 * @details steps, each set merges one more pair of groups than the set before:
 * - each queue is a group
 * - groups with equal properties merge
 * - a group merges into a group whose combined properties contain its properties
 * - all queues are one group
 * @param queues properties of each queue
 * @return the sets, each set spans all queues
 * @pre `!queues.empty()`
 */
[[nodiscard]] std::vector<cvk::Core::queue_set_t> queue_sets(std::span<cvk::QueueFamilyProperties const>);
}

namespace jvk::dev {
/**
 * @return combined properties of the group of the queue
 */
[[nodiscard]] inline cvk::QueueFamilyProperties group_properties(
    cvk::Core::queue_set_t const& set,
    std::span<cvk::QueueFamilyProperties const> queues,
    size_t queueIdx
) {
    auto const root = set.find(queueIdx);

    auto result = cvk::QueueFamilyProperties::NONE;
    for(size_t i = 0; i < queues.size(); ++i)
        if(set.find(i) == root) result |= queues[i];
    return result;
}

/**
 * appends one set per merge of two groups that match
 * @param[out] sets
 * @param match(lhs, rhs) properties of two groups, `true` -> the groups merge
 * @pre `!sets.empty()`
 */
template<class Match>
void merge_groups(
    std::vector<cvk::Core::queue_set_t>& sets,
    std::span<cvk::QueueFamilyProperties const> queues,
    Match const& match
) {
    for(size_t i = queues.size() - 1; i > 0; --i)
        for(size_t j = i; j-- > 0;) {
            auto const& set = sets.back();
            if(set.find(j) == set.find(i)) continue;
            if(!match(group_properties(set, queues, j), group_properties(set, queues, i))) continue;

            auto next = set;
            next.merge(j, i);
            sets.push_back(std::move(next));
            break;
        }
}
}

namespace jvk {
inline std::vector<cvk::Core::queue_set_t> queue_sets(std::span<cvk::QueueFamilyProperties const> queues) {
    CTH_CRITICAL(queues.empty(), "at least one queue is required") {}

    std::vector<cvk::Core::queue_set_t> sets{};
    sets.emplace_back(queues.size());

    dev::merge_groups(sets, queues, std::equal_to{});
    dev::merge_groups(
        sets,
        queues,
        [](cvk::QueueFamilyProperties lhs, cvk::QueueFamilyProperties rhs) {
            return contains(lhs, rhs) || contains(rhs, lhs);
        }
    );

    if(sets.back().roots().size() > 1) {
        auto all = sets.back();
        for(size_t i = 1; i < queues.size(); ++i)
            all.merge(0, i);
        sets.push_back(std::move(all));
    }

    return sets;
}
}
