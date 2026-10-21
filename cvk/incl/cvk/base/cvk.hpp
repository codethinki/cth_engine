#pragma once
#include <cth/coro/global_switch.hpp>

namespace cvk {
/**
 * @brief loads the vulkan loader while a Cvk exists, neither copyable nor movable
 * @details reference counted like std::ios_base::Init: every Cvk shares the one loader of the process,
 *  the first Cvk loads it, the last one unloads it,
 *  the Cvk outlives every instance
 * @attention not thread safe
 */
class Cvk {
public:
    /**
     * @brief the first Cvk loads the vulkan loader through volk
     * @post @ref initialized()
     * @throws cvk::vk_result_exception result of volkInitialize()
     */
    Cvk();

    /**
     * @brief the last Cvk unloads the vulkan loader
     * @pre the last Cvk -> no instance exists
     */
    ~Cvk();

    /**
     * @return a Cvk exists
     */
    [[nodiscard]] static bool initialized() { return _loader.acquired(); }

private:
    /**
     * @throws cvk::vk_result_exception result of volkInitialize()
     */
    static void load();
    static void unload();

    inline static cth::co::global_switch _loader{&load, &unload};

public:
    Cvk(Cvk const& other) = delete;
    Cvk(Cvk&& other) = delete;
    Cvk& operator=(Cvk const& other) = delete;
    Cvk& operator=(Cvk&& other) = delete;
};
}
