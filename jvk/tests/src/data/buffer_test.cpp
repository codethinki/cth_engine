#include "jvk/data/buffer.hpp"

#include "context.hpp"
#include "test.hpp"

#include "jvk/cmd/cmd_buffer.hpp"

#include <array>
#include <cstdint>


namespace jvk {
using test::READBACK_CONFIG;
using test::STAGING_CONFIG;

namespace {
    constexpr std::array<uint32_t, 4> VALUES{1, 2, 3, 4};

    constexpr cvk::BaseBuffer::Config DEVICE_CONFIG{
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    constexpr BufferAccess HOST_READ{VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
}

JVK_TEST(Buffer, copies_record_their_accesses) {
    test::Context const context{};
    Buffer<uint32_t> staging{context.core, STAGING_CONFIG, VALUES.size()};
    staging.write(VALUES);
    Buffer<uint32_t> const device{context.core, DEVICE_CONFIG, VALUES.size()};
    Buffer<uint32_t> const readback{context.core, READBACK_CONFIG, VALUES.size()};
    PrimaryCmdBuffer primary{context.core, test::queue_family(context)};

    primary.begin();
    device.copy(primary, staging);
    readback.copy(primary, device);
    primary.use(readback, HOST_READ);
    primary.end();

    EXPECT_EQ(primary.states().first(device)->scope.access, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    EXPECT_EQ(primary.states().last(device)->scope.access, VK_ACCESS_2_TRANSFER_READ_BIT);
    EXPECT_EQ(primary.states().last(staging)->scope.access, VK_ACCESS_2_TRANSFER_READ_BIT);

    test::submit_and_wait(context, primary);

    auto const mapping = readback.map();
    readback.invalidate();
    for(size_t i = 0; i < VALUES.size(); ++i)
        EXPECT_EQ(mapping[i], VALUES[i]);
}
}
