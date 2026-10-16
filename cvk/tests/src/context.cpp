#include "context.hpp"

#include "validation.hpp"


namespace cvk::test {
Core::Config const Context::CONFIG{
    .queueProperties = {QueueFamilyProperties::GRAPHICS | QueueFamilyProperties::COMPUTE},
};

Context::Context(Core::Config const& config) : instance{APP_NAME, NO_EXTENSIONS, counting_messenger()},
    core{instance, {}, config} {}

Context::ValidationCheck::ValidationCheck() { reset_validation_errors(); }

Context::ValidationCheck::~ValidationCheck() { EXPECT_EQ(validation_errors(), 0); }
}
