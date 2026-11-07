#include "jvk/base/queues.hpp"

#include "context.hpp"
#include "test.hpp"

#include "cvk/base/core.hpp"


namespace jvk {
JVK_TEST(Queues, queue_refers_to_its_cvk_queue) {
    test::Context const context{};
    cvk::Core const& vk = context.core;

    Queues const queues{vk, 0};

    EXPECT_EQ(&queues.queue(0).vk(), &vk.queue(0));
    EXPECT_EQ(queues.queue(0).get(), vk.queue(0).get());
    queues.queue(0).waitIdle();
}
}
