#include "cvk/base/cvk.hpp"

#include "cvk/utility/vk_exceptions.hpp"

#include <cth/io/log.hpp>


namespace cvk {
Cvk::Cvk() { _loader.acquire(); }

Cvk::~Cvk() { _loader.release(); }

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
