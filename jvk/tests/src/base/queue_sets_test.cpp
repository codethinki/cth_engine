#include "jvk/base/queue_sets.hpp"

#include "test.hpp"

#include <cstddef>
#include <ranges>
#include <span>
#include <vector>


namespace jvk {
namespace {
    using enum cvk::QueueFamilyProperties;

    using groups_t = std::vector<std::vector<size_t>>;

    /**
     * @return groups of each queue set
     */
    std::vector<groups_t> set_groups(std::span<cvk::QueueFamilyProperties const> queues) {
        auto sets = queue_sets(queues);
        return {std::from_range, sets | std::views::transform([](auto& set) { return set.all(); })};
    }
}

JVK_TEST(queue_sets, one_queue_is_one_set) {
    std::vector const queues{GRAPHICS};

    EXPECT_EQ(set_groups(queues), (std::vector<groups_t>{{{0}}}));
}

JVK_TEST(queue_sets, disjoint_queues_separate_then_all_in_one) {
    std::vector const queues{GRAPHICS, COMPUTE, TRANSFER};

    EXPECT_EQ(set_groups(queues), (std::vector<groups_t>{{{0}, {1}, {2}}, {{0, 1, 2}}}));
}

JVK_TEST(queue_sets, equal_queues_merge_also_when_not_adjacent) {
    std::vector const queues{GRAPHICS, COMPUTE, GRAPHICS};

    EXPECT_EQ(set_groups(queues), (std::vector<groups_t>{{{0}, {1}, {2}}, {{0, 2}, {1}}, {{0, 1, 2}}}));
}

JVK_TEST(queue_sets, equal_before_subset) {
    std::vector const queues{GRAPHICS | COMPUTE, TRANSFER, COMPUTE, TRANSFER};

    EXPECT_EQ(
        set_groups(queues),
        (std::vector<groups_t>{
            {{0}, {1}, {2}, {3}},
            {{0}, {1, 3}, {2}},
            {{0, 2}, {1, 3}},
            {{0, 1, 2, 3}},
        })
    );
}

JVK_TEST(queue_sets, subset_merges_in_both_directions) {
    std::vector const queues{TRANSFER, COMPUTE, GRAPHICS | COMPUTE};

    EXPECT_EQ(set_groups(queues), (std::vector<groups_t>{{{0}, {1}, {2}}, {{0}, {1, 2}}, {{0, 1, 2}}}));
}

JVK_TEST(queue_sets, subset_uses_the_combined_properties_of_a_group) {
    std::vector const queues{GRAPHICS, COMPUTE, GRAPHICS | COMPUTE};

    EXPECT_EQ(set_groups(queues), (std::vector<groups_t>{{{0}, {1}, {2}}, {{0}, {1, 2}}, {{0, 1, 2}}}));
}

JVK_TEST(queue_sets, no_duplicate_sets) {
    std::vector const queues{GRAPHICS, GRAPHICS};

    EXPECT_EQ(set_groups(queues), (std::vector<groups_t>{{{0}, {1}}, {{0, 1}}}));
}

JVK_TEST(queue_sets, every_set_spans_all_queues) {
    std::vector const queues{GRAPHICS | PRESENT, COMPUTE, TRANSFER, GRAPHICS, COMPUTE | TRANSFER};

    auto sets = queue_sets(queues);

    ASSERT_FALSE(sets.empty());
    for(auto const& set : sets)
        EXPECT_EQ(set.size(), queues.size());
    EXPECT_EQ(sets.front().roots().size(), queues.size());
    EXPECT_EQ(sets.back().roots().size(), 1);
}
}
