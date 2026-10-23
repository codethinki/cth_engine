#include "context.hpp"

#include "cvk/base/cvk.hpp"

#include <gtest/gtest.h>

#include <optional>


namespace cvk::test {
namespace {
    /**
     * @brief holds the vulkan loader for all tests
     * @details the shared contexts are destroyed before the loader
     */
    class Environment : public testing::Environment {
    public:
        void SetUp() override { _cvk.emplace(); }
        void TearDown() override {
            destroy_shared_contexts();
            _cvk.reset();
        }

    private:
        std::optional<Cvk> _cvk{};
    };

    [[maybe_unused]] testing::Environment* const ENVIRONMENT = testing::AddGlobalTestEnvironment(new Environment{});
}
}
