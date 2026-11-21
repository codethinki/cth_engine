#include "cvk/base/cvk.hpp"

#include "cvk/utility/vk_exceptions.hpp"

#include <cth/coro/global_switch.hpp>
#include <cth/io/log.hpp>


namespace cvk {
Cvk::Cvk() { loader().acquire(); }

Cvk::~Cvk() { loader().release(); }

bool Cvk::initialized() { return loader().acquired(); }

cth::co::global_switch& Cvk::loader() {
    // constructed at the first call, also when the first call comes from the static initialization of another unit
    static cth::co::global_switch loaderSwitch{&load, &unload};
    return loaderSwitch;
}

void Cvk::load() {
    auto const result = volkInitialize();
    CVK_RESULT_STABLE_THROW(result != VK_SUCCESS, result, "failed to load vulkan") {}

    log::msg<except::INFO>("cvk initialized, loaded vulkan (volk)");
}

void Cvk::unload() {
    volkFinalize();

    log::msg<except::INFO>("cvk terminated, unloaded vulkan (volk)");
}
}
