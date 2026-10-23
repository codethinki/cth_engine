#pragma once
#include <volk.h>

#include <cstdint>
#include <string>
#include <vector>


namespace cvk {
/**
 * @brief application, engine and extensions of an instance
 */
struct InstanceConfig {
    std::string appName;
    uint32_t appVersion = 0;
    std::string engineName = "cvk";
    uint32_t engineVersion = 0;

    /**
     * @brief instance extensions, debug builds add @ref Instance::VALIDATION_LAYER_EXTENSIONS
     */
    std::vector<std::string> extensions{};

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
