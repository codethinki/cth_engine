#pragma once

namespace cvk {
/**
 * @brief loads and unloads the vulkan loader
 */
class Cvk {
public:
    /**
     * @brief loads the vulkan loader through volk
     * @post @ref initialized()
     * @throws cvk::vk_result_exception result of volkInitialize()
     */
    static void init();

    /**
     * @brief unloads the vulkan loader
     * @post !@ref initialized()
     */
    static void terminate();

private:
    inline static bool _initialized = false;
    static void terminateVolk();
    static void initVolk();

public:
    [[nodiscard]] static bool initialized() { return _initialized; }
};
}
