#pragma once
#include "cvk/debug/debug_messenger.hpp"

#include <volk.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace cvk {
/**
 * application, engine, extensions and messenger of an instance
 */
struct InstanceConfig {
    std::string appName;
    uint32_t appVersion = 0;
    std::string engineName = "cvk";
    uint32_t engineVersion = 0;

    /**
     * instance extensions, a messenger adds @ref Instance::MESSENGER_EXTENSIONS
     */
    std::vector<std::string> extensions{};

    /**
     * value -> messenger with this config
     * @details debug builds (validation layers on) always have a messenger, std::nullopt -> the default config
     */
    std::optional<DebugMessenger::Config> debugMessenger = std::nullopt;

    /**
     * @attention the names point into the config
     */
    [[nodiscard]] constexpr VkApplicationInfo appInfo() const {
        return VkApplicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = appName.c_str(),
            .applicationVersion = appVersion,
            .pEngineName = engineName.c_str(),
            .engineVersion = engineVersion,
            .apiVersion = VK_API_VERSION_1_3,
        };
    }
};
}
