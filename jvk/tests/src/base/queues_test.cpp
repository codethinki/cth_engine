#include "jvk/base/queues.hpp"

#include "context.hpp"
#include "test.hpp"

#include "cvk/base/core.hpp"

#include <cth/coro/scheduler.hpp>


namespace jvk {
JVK_TEST(Queues, queue_refers_to_its_cvk_queue) {
    test::Context const context{};
    cvk::Core const& vk = context.core;
    cth::co::scheduler const workers{cth::co::autostart};

    Queues const queues{context.core, workers, 0};

    EXPECT_EQ(&queues.queue(0).vk(), &vk.queue(0));
    EXPECT_EQ(queues.queue(0).get(), vk.queue(0).get());
    queues.queue(0).waitIdle();
}
}
