#pragma once
#include "test.hpp"

#include "cvk/base/instance.hpp"
#include "cvk/debug/debug_messenger.hpp"



namespace cgfw::test {
/**
 * @brief messenger config that logs every message and counts the errors
 * @details a small copy of the cvk tests, the instance create info gets it too:
 *  counts the errors of vkDestroyInstance(), e.g. leaked surfaces
 */
[[nodiscard]] cvk::DebugMessenger::Config counting_messenger();

/**
 * @brief resets the error count, the destructor fails the test on errors
 */
struct ValidationCheck {
    ValidationCheck();
    ~ValidationCheck();
};

/**
 * @brief own instance with @ref instance_extensions() and @ref counting_messenger(), fails the test on validation errors
 * @details the check runs after the instance is destroyed
 */
struct InstanceContext {
    InstanceContext();

    ValidationCheck validationCheck;
    cvk::Instance instance;

    InstanceContext(InstanceContext const& other) = delete;
    InstanceContext(InstanceContext&& other) = delete;
    InstanceContext& operator=(InstanceContext const& other) = delete;
    InstanceContext& operator=(InstanceContext&& other) = delete;
};
}
