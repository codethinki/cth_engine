#include "context.hpp"

#include "cvk/base/cvk.hpp"

#include <gtest/gtest.h>


namespace cvk::test {
namespace {
    /**
     * @brief loads the vulkan loader once for all tests
     */
    class Environment : public testing::Environment {
    public:
        void SetUp() override { Cvk::init(); }
        void TearDown() override {
            destroy_shared_contexts();
            Cvk::terminate();
        }
    };

    [[maybe_unused]] testing::Environment* const ENVIRONMENT = testing::AddGlobalTestEnvironment(new Environment{});
}
}
