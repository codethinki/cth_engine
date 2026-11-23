#pragma once

namespace cth::co {
class global_switch;
}

namespace cvk {
/**
 * vulkan loader guard
 * @details the first `Cvk` loads the loader, the last one unloads it
 * @details thread safe
 * @details outlives every instance
 */
class Cvk {
public:
    /**
     * the first `Cvk` loads the vulkan loader through volk
     * @post `initialized()`
     * @throws cvk::vk_result_exception result of `volkInitialize()`
     */
    Cvk();

    /**
     * the last `Cvk` unloads the vulkan loader
     * @pre the last `Cvk` -> no instance exists
     */
    ~Cvk();

    /**
     * @return a `Cvk` exists
     */
    [[nodiscard]] static bool initialized();

private:
    [[nodiscard]] static cth::co::global_switch& loader();

    static void load();
    static void unload();

public:
    Cvk(Cvk const& other) = delete;
    Cvk(Cvk&& other) = delete;
    Cvk& operator=(Cvk const& other) = delete;
    Cvk& operator=(Cvk&& other) = delete;
};
}
