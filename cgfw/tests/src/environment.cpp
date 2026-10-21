#include "cvk/base/cvk.hpp"

#include <gtest/gtest.h>

#include <optional>


namespace cgfw::test {
namespace {
    /**
     * @brief holds the vulkan loader for all tests
     */
    class Environment : public testing::Environment {
    public:
        void SetUp() override { _cvk.emplace(); }
        void TearDown() override { _cvk.reset(); }

    private:
        std::optional<cvk::Cvk> _cvk{};
    };

    [[maybe_unused]] testing::Environment* const ENVIRONMENT = testing::AddGlobalTestEnvironment(new Environment{});
}
}
